#include "WordProblem.h"
#include "MathEngine.h"
#include "BigNum.h"
#include "NaturalLanguage.h"

#include <QRegularExpression>
#include <QStringList>
#include <QMap>

namespace {

// -- Sentence splitting -------------------------------------------------------
// Split on . ? ! but only when the terminator is followed by end-of-string or
// whitespace + a capital. That keeps abbreviations like "no. of apples" intact,
// which matters because those are exactly the phrase names we support.
QStringList splitSentences(const QString& text) {
    static const QRegularExpression boundary(
        R"([.?!]+(?=\s+[A-Z]|\s*$))");
    QStringList out;
    int last = 0;
    auto it = boundary.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        out << text.mid(last, m.capturedEnd() - last);
        last = m.capturedEnd();
    }
    if (last < text.size()) out << text.mid(last);

    QStringList cleaned;
    for (QString s : out) {
        s = s.trimmed();
        while (!s.isEmpty() && (s.endsWith('.') || s.endsWith('?') || s.endsWith('!')))
            s.chop(1);
        s = s.trimmed();
        if (!s.isEmpty()) cleaned << s;
    }
    return cleaned;
}

// -- Quantities ---------------------------------------------------------------
// Accepts digits ("5") and number words ("five", "twenty five") by routing the
// latter through NaturalLanguage, so we reuse its number-word tables rather than
// duplicating them.
bool parseNumber(const QString& tok, QString& outNumeric) {
    const QString t = tok.trimmed();
    if (t.isEmpty()) return false;

    bool ok = false;
    BigNum::bigEvalValue(t, ok);
    if (ok) { outNumeric = t; return true; }

    const QString pre = NaturalLanguage::preprocess(t).trimmed();
    if (pre.isEmpty() || pre.compare(t, Qt::CaseInsensitive) == 0) return false;
    BigNum::bigEvalValue(pre, ok);
    if (ok) { outNumeric = pre; return true; }
    return false;
}

// "5 red apples" -> qty "5", rest "red apples". Longest numeric prefix wins so
// multi-word numbers ("twenty five apples") bind correctly.
bool splitLeadingNumber(const QString& s, QString& qty, QString& rest) {
    const QStringList toks = s.split(' ', Qt::SkipEmptyParts);
    for (int n = toks.size() - 1; n >= 1; --n) {
        const QString cand = QStringList(toks.mid(0, n)).join(' ');
        QString num;
        if (parseNumber(cand, num)) {
            qty = num;
            rest = QStringList(toks.mid(n)).join(' ');
            return !rest.isEmpty();
        }
    }
    return false;
}

QString norm(const QString& s) { return s.simplified().toLower(); }

// -- Noun normalisation -------------------------------------------------------
// Fold "apple" and "apples" onto one key so a problem can say "1 apple" and then
// ask "how many apples". This is NORMALISATION, not pluralisation: it never has
// to produce good English, it only has to map both forms of a word to the same
// string. That makes a wrong-but-consistent stem harmless, which is why a short
// rule table is enough where real pluralisation would need a dictionary.
//
// The one genuinely ambiguous family is left alone on purpose: "-ses" can be
// stem+s ("roses") or stem+es ("buses"), and you cannot tell which without
// knowing the singular. We favour the commoner reading (drop one 's'), so
// "roses"/"houses" fold correctly and "bus"/"buses" only misses if a single
// problem mixes both forms.
QString singularize(const QString& wordIn) {
    const QString w = wordIn.toLower();
    if (w.size() < 3) return w;

    static const QMap<QString, QString> irregular{
        { "children","child" }, { "men","man" },       { "women","woman" },
        { "feet","foot" },      { "teeth","tooth" },   { "geese","goose" },
        { "mice","mouse" },     { "people","person" }, { "dice","die" },
        { "knives","knife" },   { "lives","life" },    { "wives","wife" },
        { "leaves","leaf" },    { "halves","half" },   { "loaves","loaf" },
        { "shelves","shelf" },  { "wolves","wolf" },
        { "potatoes","potato" },{ "tomatoes","tomato" },{ "heroes","hero" },
        { "sheep","sheep" },    { "fish","fish" },     { "series","series" },
    };
    if (irregular.contains(w)) return irregular.value(w);

    // Singular words that already end in 's' must never be stripped, or the
    // plural ("glasses" -> "glass") would stop matching the singular.
    if (w.endsWith(QStringLiteral("ss"))) return w;

    if (w.endsWith(QStringLiteral("ies")) && w.size() > 4)
        return w.left(w.size() - 3) + QLatin1Char('y');
    // Sibilant stems take "-es": glass/glasses, box/boxes, watch/watches.
    if (w.endsWith(QStringLiteral("sses")) || w.endsWith(QStringLiteral("xes"))
        || w.endsWith(QStringLiteral("zes")) || w.endsWith(QStringLiteral("ches"))
        || w.endsWith(QStringLiteral("shes")))
        return w.left(w.size() - 2);
    if (w.endsWith(QLatin1Char('s'))
        && !w.endsWith(QStringLiteral("us")) && !w.endsWith(QStringLiteral("is")))
        return w.left(w.size() - 1);
    return w;
}

// Only the head noun carries number: "red apples" -> "red apple".
QString canonAttribute(const QString& attribute) {
    QStringList toks = norm(attribute).split(' ', Qt::SkipEmptyParts);
    if (toks.isEmpty()) return QString();
    toks.last() = singularize(toks.last());
    return toks.join(' ');
}

// Canonical key - what facts and questions are matched on.
QString keyFor(const QString& attribute, const QString& owner) {
    return canonAttribute(attribute) + QStringLiteral(" with ") + norm(owner);
}

// The key exactly as the user worded it. Shown in the working, and registered
// alongside the canonical one so either spelling resolves later.
QString writtenKeyFor(const QString& attribute, const QString& owner) {
    return norm(attribute) + QStringLiteral(" with ") + norm(owner);
}

// -- Facts --------------------------------------------------------------------
enum class Kind { Direct, More, Less, Times };

struct Fact {
    Kind    kind = Kind::Direct;
    QString owner;
    QString attribute;
    QString qty;         // numeric string
    QString other;       // referenced owner, for relational kinds
    QString sentence;
};

bool matchFact(const QString& sentence, Fact& f) {
    // Relational forms are checked before the plain one: "has 3 more apples
    // than Timmy" also matches "has <something>", so the specific patterns must
    // get first refusal.
    static const QRegularExpression relRe(
        R"(^\s*([A-Za-z][\w']*)\s+has\s+(.+?)\s+(more|fewer|less)\s+(.+?)\s+than\s+([A-Za-z][\w']*)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression timesRe(
        R"(^\s*([A-Za-z][\w']*)\s+has\s+(.+?)\s+times\s+as\s+many\s+(.+?)\s+as\s+([A-Za-z][\w']*)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression hasRe(
        R"(^\s*([A-Za-z][\w']*)\s+(?:has|had|have)\s+(.+?)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    if (auto m = timesRe.match(sentence); m.hasMatch()) {
        QString qty;
        if (!parseNumber(m.captured(2), qty)) return false;
        f = { Kind::Times, m.captured(1), m.captured(3), qty, m.captured(4), sentence };
        return true;
    }
    if (auto m = relRe.match(sentence); m.hasMatch()) {
        QString qty;
        if (!parseNumber(m.captured(2), qty)) return false;
        const QString dir = m.captured(3).toLower();
        f = { dir == "more" ? Kind::More : Kind::Less,
              m.captured(1), m.captured(4), qty, m.captured(5), sentence };
        return true;
    }
    if (auto m = hasRe.match(sentence); m.hasMatch()) {
        QString qty, attr;
        if (!splitLeadingNumber(m.captured(2), qty, attr)) return false;
        f = { Kind::Direct, m.captured(1), attr, qty, QString(), sentence };
        return true;
    }
    return false;
}

// -- Questions ----------------------------------------------------------------
enum class QKind { Lookup, Total, Difference };

struct Query {
    QKind   kind = QKind::Lookup;
    QString attribute;
    QString owner;
    QString other;
};

bool matchQuestion(const QString& sentence, Query& q) {
    static const QRegularExpression diffRe(
        R"(^\s*how\s+many\s+more\s+(.+?)\s+(?:does|do)\s+([A-Za-z][\w']*)\s+have\s+than\s+([A-Za-z][\w']*)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression totalRe(
        R"(^\s*how\s+many\s+(.+?)\s+(?:do|does|are)\s+.*?\b(?:altogether|in\s+total|in\s+all|combined|together)\b\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression lookupRe(
        R"(^\s*how\s+many\s+(.+?)\s+(?:does|do)\s+([A-Za-z][\w']*)\s+have\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    if (auto m = diffRe.match(sentence); m.hasMatch()) {
        q = { QKind::Difference, m.captured(1), m.captured(2), m.captured(3) };
        return true;
    }
    if (auto m = totalRe.match(sentence); m.hasMatch()) {
        q = { QKind::Total, m.captured(1), QString(), QString() };
        return true;
    }
    if (auto m = lookupRe.match(sentence); m.hasMatch()) {
        q = { QKind::Lookup, m.captured(1), m.captured(2), QString() };
        return true;
    }
    return false;
}

// A quantity bound by an EARLIER input. Facts are written to the variable
// registry as they are learned, so a question can be asked on its own line.
QString remembered(const QString& key, bool& found) {
    const QString v = MathEngine::variableValue(key);
    found = !v.isEmpty();
    return v;
}

// Every owner carrying this attribute, from the registry. Only canonical keys
// are counted: each fact is registered under BOTH its canonical and its
// as-worded name, so counting every match would double the total.
QStringList rememberedAll(const QString& attr) {
    QStringList out;
    const QString prefix = attr + QStringLiteral(" with ");
    for (const QString& k : MathEngine::definedVariableNames()) {
        if (!k.startsWith(prefix)) continue;
        if (keyFor(attr, k.mid(prefix.size())) != k) continue;   // skip alias
        out << MathEngine::variableValue(k);
    }
    return out;
}

QString evalToString(const QString& expr, bool& ok) {
    const BigDec v = BigNum::bigEvalValue(expr, ok);
    return ok ? BigNum::fmt(v) : QString();
}

} // namespace

namespace WordProblem {

bool looksLikeWordProblem(const QString& text) {
    // EITHER is enough. Requiring both meant a bare fact ("timmy has five
    // apples") fell through to the arithmetic evaluator, which then complained
    // that "timmy" and "apples" are not numbers. Stating a fact on its own is a
    // perfectly reasonable thing to type, and it lets a problem be built up over
    // several lines before anything is asked.
    //
    // The fact pattern is ANCHORED (a name, then has/have/had) rather than a
    // loose search for "has", so ordinary expressions still never qualify.
    static const QRegularExpression factRe(
        R"(^\s*[A-Za-z][\w']*\s+(?:has|have|had)\b)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression askRe(R"(\bhow\s+many\b)",
        QRegularExpression::CaseInsensitiveOption);
    return factRe.match(text).hasMatch() || askRe.match(text).hasMatch();
}

Solution solve(const QString& text) {
    Solution sol;
    const QStringList sentences = splitSentences(text);
    if (sentences.isEmpty()) { sol.failure = QStringLiteral("Nothing to read."); return sol; }

    QVector<Fact> facts;
    Query query;
    bool haveQuery = false;

    for (const QString& s : sentences) {
        Query q;
        if (matchQuestion(s, q)) {
            query = q; haveQuery = true; sol.question = s;
            continue;
        }
        Fact f;
        if (matchFact(s, f)) { facts << f; continue; }

        // Fail loudly rather than guess.
        sol.failure = QStringLiteral("I didn't understand: \"%1\"").arg(s);
        return sol;
    }

    if (facts.isEmpty() && !haveQuery) {
        sol.failure = QStringLiteral("No facts to work from.");
        return sol;
    }

    // Resolve facts in the order stated; relational ones look back at owners
    // already known.
    QMap<QString, QString> values;   // key -> numeric string
    for (const Fact& f : facts) {
        const QString key = keyFor(f.attribute, f.owner);            // canonical
        const QString shown = writtenKeyFor(f.attribute, f.owner);   // as worded
        QString expr;      // parenthesised, for evaluation
        QString dispExpr;  // bare, for the working shown to the user

        if (f.kind == Kind::Direct) {
            expr = dispExpr = f.qty;
        }
        else {
            const QString otherKey = keyFor(f.attribute, f.other);
            // The owner being referred to may have been bound by an EARLIER
            // input ("timmy has five apples" on its own line, then "sarah has 3
            // more apples than timmy"). Consult memory before giving up.
            if (!values.contains(otherKey)) {
                bool found = false;
                const QString v = remembered(otherKey, found);
                if (found) values[otherKey] = v;
            }
            if (!values.contains(otherKey)) {
                sol.failure = QStringLiteral(
                    "\"%1\" refers to %2, but I don't know how many %3 %2 has yet.")
                    .arg(f.sentence, norm(f.other), norm(f.attribute));
                return sol;
            }
            const QString base = values.value(otherKey);
            const QChar op = f.kind == Kind::More ? '+' : f.kind == Kind::Less ? '-' : '*';
            expr = QStringLiteral("(%1) %2 (%3)").arg(base, QString(op), f.qty);
            dispExpr = QStringLiteral("%1 %2 %3").arg(base, QString(op), f.qty);
        }

        bool ok = false;
        const QString value = evalToString(expr, ok);
        if (!ok) {
            sol.failure = QStringLiteral("I couldn't work out a number from \"%1\".").arg(f.sentence);
            return sol;
        }
        values[key] = value;
        // Register both spellings so "apple with timmy" and "apples with timmy"
        // both resolve in later expressions.
        MathEngine::setVariable(key, value);
        if (shown != key) MathEngine::setVariable(shown, value);

        Step st;
        st.sentence = f.sentence;
        st.binding = QStringLiteral("%1 = %2").arg(shown, dispExpr);
        st.value = value;
        sol.working << st;
    }

    // Facts with no question: the bindings ARE the answer. Report them and stop.
    if (!haveQuery) {
        sol.solved = true;
        sol.answer = sol.working.isEmpty() ? QString() : sol.working.last().value;
        return sol;
    }

    // A question asked on its own line resolves against what earlier inputs
    // bound. When this input carried its own facts we deliberately use ONLY
    // those, so a self-contained problem can't be contaminated by a stale
    // quantity left over from an unrelated one.
    const bool fromMemory = facts.isEmpty();

    // Answer the question. `attr` is the canonical form used for matching;
    // `attrShown` is the user's own wording, for anything they will read.
    const QString attr = canonAttribute(query.attribute);
    const QString attrShown = norm(query.attribute);
    bool ok = false;

    if (query.kind == QKind::Lookup) {
        const QString key = keyFor(attr, query.owner);
        if (fromMemory) {
            bool found = false;
            const QString v = remembered(key, found);
            if (found) { values[key] = v; }
        }
        if (!values.contains(key)) {
            sol.failure = QStringLiteral("I don't know how many %1 %2 has.")
                .arg(attrShown, norm(query.owner));
            return sol;
        }
        sol.answer = values.value(key);
        sol.detail = key;
    }
    else if (query.kind == QKind::Difference) {
        const QString a = keyFor(attr, query.owner);
        const QString b = keyFor(attr, query.other);
        if (fromMemory) {
            bool fa = false, fb = false;
            const QString va = remembered(a, fa), vb = remembered(b, fb);
            if (fa) values[a] = va;
            if (fb) values[b] = vb;
        }
        if (!values.contains(a) || !values.contains(b)) {
            sol.failure = QStringLiteral("I don't have %1 for both %2 and %3.")
                .arg(attrShown, norm(query.owner), norm(query.other));
            return sol;
        }
        const QString expr = QStringLiteral("(%1) - (%2)").arg(values.value(a), values.value(b));
        sol.answer = evalToString(expr, ok);
        if (!ok) { sol.failure = QStringLiteral("Couldn't compute the difference."); return sol; }
        sol.detail = QStringLiteral("%1 - %2").arg(values.value(a), values.value(b));
    }
    else { // Total
        QStringList parts;
        if (fromMemory) {
            parts = rememberedAll(attr);
        }
        else {
            for (const Fact& f : facts)
                if (canonAttribute(f.attribute) == attr)
                    parts << values.value(keyFor(f.attribute, f.owner));
        }
        if (parts.isEmpty()) {
            sol.failure = QStringLiteral("I don't have any %1 to add up.").arg(attrShown);
            return sol;
        }
        sol.answer = evalToString(parts.join(" + "), ok);
        if (!ok) { sol.failure = QStringLiteral("Couldn't add those up."); return sol; }
        sol.detail = parts.join(" + ");
    }

    sol.solved = true;
    return sol;
}

QString format(const Solution& s) {
    if (!s.solved) return s.failure;

    QStringList lines;
    for (const Step& st : s.working) {
        // A direct fact already ends in its own value ("... = 5"); don't repeat it.
        if (st.binding.endsWith(QStringLiteral("= ") + st.value))
            lines << QStringLiteral("%1  ->  %2").arg(st.sentence, st.binding);
        else
            lines << QStringLiteral("%1  ->  %2 = %3").arg(st.sentence, st.binding, st.value);
    }
    // Facts stated with nothing asked: the bindings are the whole output.
    if (s.question.isEmpty()) return lines.join('\n');

    if (!s.detail.isEmpty())
        lines << QStringLiteral("%1  ->  %2 = %3").arg(s.question, s.detail, s.answer);
    lines << QStringLiteral("Answer: %1").arg(s.answer);
    return lines.join('\n');
}

} // namespace WordProblem
