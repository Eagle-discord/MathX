#pragma once
#include <QString>
#include <QStringList>
#include <QSet>
#include <algorithm>
#include "../math/MathEngine.h"
#include "../shapes/ShapeDefs.h"

// -- Completer -----------------------------------------------------------------
// Given what the user has typed, returns the single best completion for the
// token under the cursor -- or an empty string when there's nothing worth
// suggesting.
//
// Owns no state, in the same spirit as InputRouter: callers pass in the context
// (the text, the history) and get an answer back. That keeps it testable without
// standing up a MainWindow, and means the ghost-text painter and the Tab handler
// can't disagree about what the suggestion is -- they both ask this.
//
// -- Design notes --------------------------------------------------------------
//
// ONE suggestion, not a list. A dropdown of candidates is a different feature
// with different ergonomics (arrow keys, mouse, dismissal); ghost text answers
// "what will Tab do" and must therefore be unambiguous. When several candidates
// tie we suggest NOTHING rather than guess -- a ghost that's wrong half the time
// is worse than no ghost, because you stop trusting it and start reading every
// suggestion, which costs more attention than typing the word.
//
// Priority order matters and isn't arbitrary:
//   1. History  -- an exact prefix match on something you actually ran is the
//                  strongest possible signal. "2 + " -> "2 + 2" only because you
//                  ran it before.
//   2. Variables/functions YOU defined -- you typed them into existence this
//      session; they're more relevant than the builtin list.
//   3. Builtins and shapes -- the long tail.
//
// History is matched on the WHOLE line; everything else on the last token. That
// asymmetry is deliberate: "2 + " has no token to complete, but it's a perfectly
// good history prefix.
// -----------------------------------------------------------------------------

namespace Completer {

    // Every builtin the tokenizer recognises. Kept here rather than reaching into
    // Expr's private static set -- that set lives inside readIdentifier() and
    // isn't exposed. A duplicated list is a liability (this codebase has five
    // such pairs and four of them had drifted), so the test below is the guard:
    // anything in this list that Expr doesn't know is a completion that produces
    // an error, which the suite would catch immediately.
    inline const QStringList& builtinNames() {
        static const QStringList names = {
            "sqrt", "cbrt", "abs", "floor", "ceil", "round", "sign",
            "sin", "cos", "tan", "asin", "acos", "atan",
            "sinr", "cosr", "tanr", "asinr", "acosr", "atanr",
            "sinh", "cosh", "tanh",
            "exp", "ln", "log", "log2", "logbase",
            "min", "max", "pow", "mod", "hypot", "fact",
            "gcd", "hcf", "lcm", "ncr", "npr",
            "factor", "factorise", "factorize", "simplify",
            "pi", "e", "tau", "ans"
        };
        return names;
    }

    // The token under the cursor: trailing run of identifier characters. Returns
    // "" when the line ends in an operator, space, or bracket -- i.e. when
    // there's no partial word to finish.
    inline QString currentToken(const QString& text) {
        int i = text.size();
        while (i > 0) {
            const QChar c = text[i - 1];
            if (!c.isLetterOrNumber() && c != '_') break;
            --i;
        }
        // A token starting with a digit is a number, not an identifier. Don't
        // suggest "2" -> "2nd" or similar nonsense.
        const QString tok = text.mid(i);
        if (!tok.isEmpty() && tok[0].isDigit()) return QString();
        return tok;
    }

    // All candidate identifiers, in priority order, deduplicated.
    //
    // THREAD SAFETY: MathEngine's variable and function registries are plain
    // unguarded statics (`static QMap<QString,QString>& variableRegistry()`),
    // written by the WORKER thread inside evaluate() and read here on the UI
    // thread on every keystroke. That is a data race -- QMap reallocates on
    // insert, so a concurrent read can walk freed nodes.
    //
    // Mitigated, not solved: we only read while the worker is idle, which the
    // caller guarantees by suppressing completion during a run (see MainWindow's
    // textChanged handler and RunState). The real fix is a mutex in MathEngine or
    // a UI-thread snapshot of the registries updated on resultReady -- worth
    // doing before anything else reads them off-thread. Recorded as a known gap
    // rather than papered over.
    inline QStringList identifierCandidates(bool includeUserDefined = true) {
        QStringList out;
        QSet<QString> seen;
        auto add = [&](const QString& s) {
            const QString l = s.toLower();
            if (!l.isEmpty() && !seen.contains(l)) { seen.insert(l); out << l; }
            };

        // Yours first -- you defined them this session, so they beat the builtins.
        if (includeUserDefined) {
            for (const QString& v : MathEngine::definedVariableNames()) add(v);
            for (const QString& f : MathEngine::definedFunctionNames()) add(f);
        }
        // Then shapes, then builtins.
        for (const auto& d : ALL_SHAPE_DEFS()) add(d.internalName);
        for (const QString& b : builtinNames()) add(b);
        return out;
    }

    // The completion for `text`, given `history` (oldest-first, as MainWindow
    // keeps it). Returns the FULL completed line, not the suffix -- callers that
    // want the ghost take .mid(text.size()).
    //
    // Returns "" when there is no unambiguous single answer.
    inline QString complete(const QString& text, const QStringList& history) {
        if (text.isEmpty()) return QString();
        // Don't fight the user mid-word-boundary: a trailing space means they
        // finished a token deliberately. History can still match (see below), but
        // identifier completion should stay quiet.
        const QString token = currentToken(text);

        // -- 1. History, whole-line prefix, most recent wins ---------------------
        // Walk backwards: the newest matching entry is the one you most likely
        // want again. Skip exact matches -- completing "2+2" to "2+2" is a no-op
        // that just makes Tab feel broken.
        for (int i = history.size() - 1; i >= 0; --i) {
            const QString& h = history[i];
            if (h.size() > text.size() && h.startsWith(text, Qt::CaseSensitive))
                return h;
        }

        // -- 2. Identifiers ------------------------------------------------------
        if (token.isEmpty()) return QString();

        const QStringList cands = identifierCandidates();

        QStringList hits;
        for (const QString& cand : cands) {
            if (cand.size() > token.size() && cand.startsWith(token, Qt::CaseInsensitive))
                hits << cand;
        }
        // An exact hit counts even though it is not in `hits` (which only holds
        // strictly-longer names). "sin" against sin/sinr/sinh, or "log" against
        // log/log2/logbase, would otherwise find a common prefix equal to what's
        // typed and give up -- leaving Tab dead on the most ordinary input there
        // is. An exact name is not ambiguous just because longer ones start the
        // same way; the user has finished typing it.
        bool exact = false;
        for (const QString& c : cands)
            if (c.compare(token, Qt::CaseInsensitive) == 0) { exact = true; break; }

        if (hits.isEmpty() && !exact) return QString();

        QString pick;
        if (exact) {
            pick = token.toLower();
        }
        else if (hits.size() == 1) {
            pick = hits.front();
        }
        else {
            // Genuinely ambiguous. Complete only the prefix every candidate
            // shares -- the same rule a shell uses, minus the list. If that adds
            // nothing, stay silent: a ghost that's wrong half the time is worse
            // than none, because you stop trusting it and start reading every
            // suggestion, which costs more attention than just typing the word.
            QString common = hits.front();
            for (const QString& h : hits) {
                int j = 0;
                const int lim = std::min(common.size(), h.size());
                while (j < lim && common[j].toLower() == h[j].toLower()) ++j;
                common.truncate(j);
                if (common.size() <= token.size()) return QString();
            }
            if (common.size() <= token.size()) return QString();
            pick = common;
        }

        // Functions want their paren. Bare names (pi, e, ans, variables) don't --
        // and neither do shapes: "circle" is followed by " r = 5", not "(".
        static const QSet<QString> noParen = { "pi", "e", "tau", "ans" };
        const bool isShape = [&] {
            for (const auto& d : ALL_SHAPE_DEFS())
                if (d.internalName.compare(pick, Qt::CaseInsensitive) == 0) return true;
            return false;
            }();
        const bool isFn = !noParen.contains(pick) && !isShape
            && (builtinNames().contains(pick) || MathEngine::isFunctionDefined(pick));

        QString completed = text.left(text.size() - token.size()) + pick;
        if (isFn) completed += '(';

        // Nothing gained -- e.g. "pi" is already complete and takes no paren.
        // Returning text unchanged would paint an empty ghost and make Tab a
        // no-op that still swallows the keypress.
        if (completed == text) return QString();
        return completed;
    }

    // Just the ghost suffix -- what to paint after the cursor.
    inline QString ghost(const QString& text, const QStringList& history) {
        const QString full = complete(text, history);
        if (full.isEmpty() || !full.startsWith(text)) return QString();
        return full.mid(text.size());
    }

} // namespace Completer