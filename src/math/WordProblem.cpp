#include "WordProblem.h"
#include "MathEngine.h"
#include "BigNum.h"
#include "NaturalLanguage.h"

#include <QRegularExpression>
#include <QStringList>
#include <QMap>
#include <QSet>
#include <algorithm>

namespace {

// -- Sentence splitting -------------------------------------------------------
// Split on . ? ! but only when the terminator is followed by end-of-string or
// whitespace + a capital. That keeps abbreviations like "no. of apples" intact,
// which matters because those are exactly the phrase names we support.
QStringList splitSentences(const QString& text) {
    // Break on a terminator followed by whitespace + ANY letter, not just a
    // capital one. Requiring a capital meant "5 apples. how many does he have?"
    // never split at all - people type the question in lower case constantly -
    // and the greedy "<name> has <rest>" pattern then swallowed the entire
    // remainder as one enormous attribute name.
    //
    // The lookbehinds protect abbreviations, which is what the capital rule was
    // really guarding: "no. of apples with timmy" is a phrase name (see
    // tryPhraseAssignment) and has to survive as one sentence.
    static const QRegularExpression boundary(
        R"((?<!\bno)(?<!\bnos)(?<!\betc)(?<!\bvs)(?<!\bapprox)(?<!\bfig)[.?!]+(?=\s+[A-Za-z]|\s*$))");
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

// -- Clause splitting ---------------------------------------------------------
// "Tom has 3 apples and Sara has 5 apples" is two facts in one sentence. Split
// on "and" / ", and" ONLY when what follows starts a new clause - a subject
// (name or pronoun) followed by a verb we recognise.
//
// That guard is the whole point. "Tom has 3 apples and 2 oranges" must NOT
// split: the right side is a bare quantity, not a clause, and splitting it would
// invent a subject-less fact. Requiring subject+verb on the right keeps
// conjoined objects intact while separating conjoined sentences.
QStringList splitClauses(const QString& sentence) {
    // Two ways a clause can open after "and":
    //   1. subject + verb   - "and Sara has 5 apples"
    //   2. bare verb        - "and gives 2 to Sara"  (subject carries over)
    // Case 2 is why the fact patterns allow an absent subject: the clause is
    // real, it just inherits its owner from the previous one.
    static const QString VERBS =
        R"(has|have|had|gives?|gave|buys?|bought|eats?|ate|drinks?|drank|)"
        R"(loses?|lost|finds?|found|takes?|took|gets?|got|receives?|received|)"
        R"(spends?|spent|sells?|sold|makes?|made|picks?|picked|adds?|added|)"
        R"(wins?|won|earns?|earned|collects?|collected|uses?|used|drops?|dropped|)"
        R"(hands?|handed|sends?|sent|keeps?|kept)";
    // Three ways clauses get joined: ", and", a bare "and", or a bare comma.
    // The bare comma is the one people actually type - "Timmy has 5 apples, he
    // eats 3 apples" - and without it that whole thing stayed a single clause.
    // ", and" is listed first so it consumes the "and" rather than leaving it
    // stranded at the head of the next clause.
    //
    // The lookahead is what keeps this safe: a comma only splits when a real
    // clause follows it (optional subject, then a verb we know). So conjoined
    // objects - "Tom has 3 apples, 2 oranges" - stay together, because "2" is
    // neither a subject nor a verb.
    static const QRegularExpression conj(
        QStringLiteral(R"(\s*(?:,\s*and\s+|,\s*|\s+and\s+))"
                       R"((?=(?:each|every)\s+[\w']+\s+(?:has|have|contains?|holds?)\b)"
                       R"(|(?:(?:[A-Z][\w']*|he|she|they|it)\s+)?(?:%1)\b))")
            .arg(VERBS),
        QRegularExpression::CaseInsensitiveOption);

    QStringList parts;
    int last = 0;
    auto it = conj.globalMatch(sentence);
    while (it.hasNext()) {
        const auto m = it.next();
        parts << sentence.mid(last, m.capturedStart() - last);
        last = m.capturedEnd();
    }
    parts << sentence.mid(last);

    QStringList out;
    for (QString p : parts) {
        p = p.trimmed();
        if (!p.isEmpty()) out << p;
    }
    return out;
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

// -- Sanity check on a parsed attribute ---------------------------------------
// An attribute is a short noun phrase: "apples", "red apples". If it arrives
// carrying sentence punctuation or half a paragraph, the splitter failed
// upstream and we are one step away from registering "apples, he eats 3 apples.
// how many does he have left" as a variable name - a confident wrong answer,
// which is precisely what the fail-loudly stance exists to prevent. Refusing
// here turns it back into a readable "I didn't understand".
//
// An EMPTY attribute is fine: the action patterns legitimately omit the noun
// ("He gives 2 to Sara") and it gets inferred later.
bool plausibleAttribute(const QString& attr) {
    const QString t = attr.trimmed();
    if (t.isEmpty()) return true;
    static const QRegularExpression punctuation(R"([.?!,;:])");
    if (t.contains(punctuation)) return false;
    return t.split(QLatin1Char(' '), Qt::SkipEmptyParts).size() <= 4;
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
// Direct/More/Less/Times state a quantity. Gain/Lose/Give CHANGE one already
// stated, so they need the owner to be known first.
enum class Kind { Direct, More, Less, Times, Gain, Lose, Give, Each };

// Owner used by "There are 12 apples" when the sentence names no one to own
// them. It is a real key like any other, so a later "How many apples are
// there?" totals it alongside any named owners.
const QString kNobody = QStringLiteral("there");

struct Fact {
    Kind    kind = Kind::Direct;
    QString owner;       // "" means "the last owner mentioned" (implied subject)
    QString attribute;   // "" means "infer it" - see inferAttr in solve()
    QString qty;         // numeric string
    QString other;       // referenced owner, for relational and Give kinds
    QString sentence;
};

// -- Action verbs -------------------------------------------------------------
// A verb is nothing but an arithmetic sign here. There is deliberately NO
// semantic checking: "ate" does not verify the object is edible, "spent" does
// not require money, "drank" does not require a liquid.
//
// The trade is explicit and worth stating. It means "Tom has 9 bricks. He ate 3
// bricks." is accepted and answers 6, which is nonsense as English and correct
// as arithmetic. Rejecting it would mean shipping a noun ontology - what is
// edible, drinkable, spendable - to catch sentences nobody types by accident,
// and every such table is wrong at the edges and has to be maintained forever.
// The user asked for the number, not a plausibility review.
//
// The payoff is that this table is the entire feature: adding "devoured" or
// "gifted" is one word in one string, no new code path, no new guard.
namespace verbs {
    // Subject loses the quantity.
    const QString LOSE =
        R"(ate|eats?|drinks?|drank|loses?|lost|spends?|spent|sells?|sold|)"
        R"(uses?|used|drops?|dropped|breaks?|broke|throws?\s+away|threw\s+away|)"
        R"(removes?|removed|gives?\s+away|gave\s+away|eats?\s+up|returns?|returned)";

    // Subject gains the quantity.
    const QString GAIN =
        R"(buys?|bought|finds?|found|gets?|got|receives?|received|picks?|picked|)"
        R"(makes?|made|adds?|added|wins?|won|earns?|earned|collects?|collected|)"
        R"(grows?|grew|catches|caught|takes?|took|borrows?|borrowed)";

    // Quantity moves from the subject to someone else.
    const QString GIVE =
        R"(gives?|gave|hands?|handed|sends?|sent|passes|passed|lends?|lent|)"
        R"(sells?\s+to|offers?|offered)";
}

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
    // "Each box has 6 apples" - a RATE, not a quantity. Checked before the
    // plain "has" form, which cannot match it anyway ("Each" is not followed by
    // a verb), but keeping the specific pattern first is the rule everywhere
    // else in this function.
    static const QRegularExpression eachRe(
        R"(^\s*(?:each|every)\s+([A-Za-z][\w']*)\s+(?:has|have|contains?|holds?)\s+(.+?)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    if (auto m = eachRe.match(sentence); m.hasMatch()) {
        QString qty, attr;
        if (splitLeadingNumber(m.captured(2), qty, attr)) {
            // `other` carries the container ("box"); the owner is whoever turns
            // out to hold the containers, decided after all facts are read.
            f = { Kind::Each, QString(), attr, qty, m.captured(1), sentence };
            return true;
        }
    }

    // "There are 12 apples in the basket" / "There are 4 boxes"
    static const QRegularExpression thereAreRe(
        R"(^\s*there\s+(?:are|is|were|was)\s+(.+?)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    if (auto m = thereAreRe.match(sentence); m.hasMatch()) {
        QString qty, rest;
        if (splitLeadingNumber(m.captured(1), qty, rest)) {
            // A trailing place becomes the owner, so "12 apples in the basket"
            // and "the basket has 12 apples" land on the same key.
            static const QRegularExpression placeRe(
                R"(^(.+?)\s+(?:in|on|at|inside)\s+(?:the\s+|a\s+|an\s+)?(.+)$)",
                QRegularExpression::CaseInsensitiveOption);
            QString attr = rest, owner = kNobody;
            if (auto pm = placeRe.match(rest); pm.hasMatch()) {
                attr = pm.captured(1).trimmed();
                owner = norm(pm.captured(2));
            }
            f = { Kind::Direct, owner, attr, qty, QString(), sentence };
            return true;
        }
    }

    if (auto m = hasRe.match(sentence); m.hasMatch()) {
        QString qty, attr;
        if (!splitLeadingNumber(m.captured(2), qty, attr)) return false;
        f = { Kind::Direct, m.captured(1), attr, qty, QString(), sentence };
        return true;
    }

    // -- Actions ---------------------------------------------------------------
    // Subject is optional throughout: a clause split off an "and" ("... and gives
    // 2 to Sara") has no subject of its own, and an empty owner means "whoever
    // was last mentioned", resolved in solve().
    const QString SUBJ = R"((?:([A-Za-z][\w']*)\s+)?)";

    // "<X> gives <N> [<things>] to <Y>"
    static const QRegularExpression giveToRe(
        QStringLiteral(R"(^\s*%1(?:%2)\s+(.+?)\s+to\s+([A-Za-z][\w']*)\s*$)")
            .arg(SUBJ, verbs::GIVE),
        QRegularExpression::CaseInsensitiveOption);
    // "<X> gives <Y> <N> [<things>]"  (dative: "Sara gives him 3 apples")
    static const QRegularExpression giveDativeRe(
        QStringLiteral(R"(^\s*%1(?:%2)\s+([A-Za-z][\w']*)\s+(.+?)\s*$)")
            .arg(SUBJ, verbs::GIVE),
        QRegularExpression::CaseInsensitiveOption);
    // "<X> eats <N> [<things>]"
    static const QRegularExpression loseRe(
        QStringLiteral(R"(^\s*%1(?:%2)\s+(.+?)\s*$)").arg(SUBJ, verbs::LOSE),
        QRegularExpression::CaseInsensitiveOption);
    // "<X> buys <N> [more] [<things>]"
    static const QRegularExpression gainRe(
        QStringLiteral(R"(^\s*%1(?:%2)\s+(.+?)\s*$)").arg(SUBJ, verbs::GAIN),
        QRegularExpression::CaseInsensitiveOption);

    // "3 more apples" / "3 apples" / "3" -> qty + (possibly empty) attribute.
    auto splitQtyAttr = [](QString rest, QString& qty, QString& attr) -> bool {
        rest = rest.trimmed();
        if (splitLeadingNumber(rest, qty, attr)) {
            static const QRegularExpression moreRe(R"(^\s*(?:more|extra|additional)\s+)",
                QRegularExpression::CaseInsensitiveOption);
            attr.remove(moreRe);
            attr = attr.trimmed();
            return true;
        }
        // Bare quantity, attribute left to inference: "He gives 2 to Sara".
        if (parseNumber(rest, qty)) { attr.clear(); return true; }
        return false;
        };

    // "gives away" is in LOSE and also starts with "gives", so the loser form
    // must be tried before the plain give forms or "gave away 2" would be read
    // as giving 2 to someone called "away".
    if (auto m = loseRe.match(sentence); m.hasMatch()) {
        QString qty, attr;
        if (splitQtyAttr(m.captured(2), qty, attr)) {
            f = { Kind::Lose, m.captured(1), attr, qty, QString(), sentence };
            return true;
        }
    }
    if (auto m = giveToRe.match(sentence); m.hasMatch()) {
        QString qty, attr;
        if (splitQtyAttr(m.captured(2), qty, attr)) {
            f = { Kind::Give, m.captured(1), attr, qty, m.captured(3), sentence };
            return true;
        }
    }
    if (auto m = giveDativeRe.match(sentence); m.hasMatch()) {
        QString qty, attr;
        if (splitQtyAttr(m.captured(3), qty, attr)) {
            f = { Kind::Give, m.captured(1), attr, qty, m.captured(2), sentence };
            return true;
        }
    }
    if (auto m = gainRe.match(sentence); m.hasMatch()) {
        QString qty, attr;
        if (splitQtyAttr(m.captured(2), qty, attr)) {
            f = { Kind::Gain, m.captured(1), attr, qty, QString(), sentence };
            return true;
        }
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

// A subject that names the whole group rather than one owner, so
// "how many do THEY have" is a total, not a lookup on someone called "they".
bool isGroupSubject(const QString& s) {
    static const QSet<QString> group{ "they", "them", "we", "us", "all", "both" };
    return group.contains(s.trimmed().toLower());
}

// Singular personal pronouns stand in for an owner already named. "they"/"them"
// are excluded on purpose - they are handled as the whole group above, and
// treating them as a back-reference to one person would silently answer a
// total question with one person's count.
bool isPronoun(const QString& s) {
    static const QSet<QString> p{ "he", "she", "it", "him", "her", "his", "hers", "its" };
    return p.contains(s.trimmed().toLower());
}

bool matchQuestion(const QString& sentence, Query& q) {
    // "how many more X does A have than B"
    static const QRegularExpression diffRe(
        R"(^\s*how\s+many\s+more\s+(.*?)\s*\b(?:does|do|did)\s+([A-Za-z][\w']*)\s+have\s+than\s+([A-Za-z][\w']*)\b.*$)",
        QRegularExpression::CaseInsensitiveOption);

    // "how many X does A have [now|left|in all|...]"
    // The attribute is optional (`.*?`) so "how many does Tom have" works, and
    // the tail after "have" is free so "have now" / "have left" do too - those
    // trailing words carry no arithmetic in this subset, they just read
    // naturally, and rejecting the sentence over them was pure friction.
    static const QRegularExpression lookupRe(
        R"(^\s*how\s+many\s+(.*?)\s*\b(?:does|do|did)\s+([A-Za-z][\w']*)\s+have\b.*$)",
        QRegularExpression::CaseInsensitiveOption);

    // "how many X are there|are in the box|are left" - existential phrasing with
    // no owner at all.
    static const QRegularExpression thereRe(
        R"(^\s*how\s+many\s+(.*?)\s*\b(?:are|is)\b.*$)",
        QRegularExpression::CaseInsensitiveOption);

    // "how many X altogether|in total|in all|combined|together", with or
    // without an intervening verb phrase. The old pattern REQUIRED
    // "do|does|are", so the extremely common "How many apples in total?" was
    // rejected outright.
    static const QRegularExpression totalRe(
        R"(^\s*how\s+many\s+(.*?)\s*(?:\b(?:do|does|did|are|is)\b.*?)?\b(?:altogether|in\s+total|in\s+all|combined|together)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    // "what is the total number of X" / "what is the total X"
    static const QRegularExpression whatTotalRe(
        R"(^\s*what\s+is\s+the\s+total\s+(?:number\s+of\s+)?(.+?)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    if (auto m = diffRe.match(sentence); m.hasMatch()) {
        q = { QKind::Difference, m.captured(1), m.captured(2), m.captured(3) };
        return true;
    }
    // Checked BEFORE the total patterns: an explicit owner wins over a trailing
    // "in all". "How many apples does Tom have in all?" is Tom's count, not the
    // sum over everyone - the old order made it a Total that happened to look
    // right only while Tom was the sole owner of apples.
    if (auto m = lookupRe.match(sentence); m.hasMatch()) {
        const QString subject = m.captured(2);
        if (isGroupSubject(subject))
            q = { QKind::Total, m.captured(1), QString(), QString() };
        else
            q = { QKind::Lookup, m.captured(1), subject, QString() };
        return true;
    }
    if (auto m = whatTotalRe.match(sentence); m.hasMatch()) {
        q = { QKind::Total, m.captured(1), QString(), QString() };
        return true;
    }
    if (auto m = totalRe.match(sentence); m.hasMatch()) {
        q = { QKind::Total, m.captured(1), QString(), QString() };
        return true;
    }
    if (auto m = thereRe.match(sentence); m.hasMatch()) {
        q = { QKind::Total, m.captured(1), QString(), QString() };
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

// A stored quantity is a count of things somebody HAS, so it cannot go below
// zero: "Timmy has 5 apples, he eats 33" is an inconsistent premise, not a
// problem with the answer -28.
//
// This is deliberately NOT the semantic checking that was ruled out (see the
// verb table). Deciding whether a brick is edible needs an ontology of nouns -
// open-ended and wrong at the edges forever. Deciding whether a count went
// negative needs one comparison. No world knowledge, so it costs nothing.
//
// Only applied to values about to be bound as somebody's count. A DIFFERENCE is
// allowed to be negative - "how many more does Sara have than Tom" is a fair
// question when Sara has fewer.
bool isNegativeCount(const QString& value) {
    return value.startsWith(QLatin1Char('-'));
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
    // The other two ways a problem can OPEN, so each is usable on its own line
    // while a problem is built up: "There are 4 boxes." and "Each box has 6
    // apples." Both are anchored and require a number, so they cannot swallow
    // ordinary input.
    static const QRegularExpression thereRe(
        R"(^\s*there\s+(?:are|is|were|was)\s+\S)",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression eachRe(
        R"(^\s*(?:each|every)\s+[A-Za-z][\w']*\s+(?:has|have|contains?|holds?)\b)",
        QRegularExpression::CaseInsensitiveOption);
    return factRe.match(text).hasMatch() || askRe.match(text).hasMatch()
        || thereRe.match(text).hasMatch() || eachRe.match(text).hasMatch();
}

Solution solve(const QString& text) {
    Solution sol;
    const QStringList sentences = splitSentences(text);
    if (sentences.isEmpty()) { sol.failure = QStringLiteral("Nothing to read."); return sol; }

    QVector<Fact> facts;
    Query query;
    bool haveQuery = false;

    for (const QString& sentence : sentences) {
        // A sentence may carry more than one clause ("Tom has 3 apples and Sara
        // has 5 apples"); each is matched independently.
        for (const QString& s : splitClauses(sentence)) {
            Query q;
            if (matchQuestion(s, q)) {
                query = q; haveQuery = true; sol.question = s;
                continue;
            }
            // One choke point for the attribute check, so every fact pattern is
            // covered rather than just the greedy "<name> has <rest>" one.
            Fact f;
            if (matchFact(s, f) && plausibleAttribute(f.attribute)) { facts << f; continue; }

            // Fail loudly rather than guess.
            sol.failure = QStringLiteral("I didn't understand: \"%1\"").arg(s);
            return sol;
        }
    }

    if (facts.isEmpty() && !haveQuery) {
        sol.failure = QStringLiteral("No facts to work from.");
        return sol;
    }

    // Resolve facts in the order stated; relational ones look back at owners
    // already known.
    QMap<QString, QString> values;   // key -> numeric string

    // -- Reference tracking ----------------------------------------------------
    // Owners in the order they were mentioned, most recent last. Pronouns and
    // omitted subjects resolve against this.
    QStringList mentioned;
    // Attributes per owner, and overall, for inferring an omitted noun.
    QMap<QString, QStringList> ownerAttrs;
    QStringList allAttrs;

    auto noteOwner = [&](const QString& o) {
        if (o.isEmpty()) return;
        mentioned.removeAll(o);
        mentioned << o;
        };
    // A pronoun or an absent subject means "someone already mentioned".
    // `exclude` keeps "Sara gives him 3 apples" from resolving "him" to Sara.
    auto resolveOwner = [&](const QString& raw, const QString& exclude) -> QString {
        const QString w = norm(raw);
        if (!w.isEmpty() && !isPronoun(w)) return w;
        for (int i = mentioned.size() - 1; i >= 0; --i)
            if (mentioned.at(i) != exclude) return mentioned.at(i);
        return QString();
        };
    // An omitted noun ("He gives 2 to Sara") is taken from what this owner
    // already has, falling back to the problem's only attribute.
    auto inferAttr = [&](const QString& given, const QString& owner) -> QString {
        if (!given.isEmpty()) return canonAttribute(given);
        const QStringList mine = ownerAttrs.value(owner);
        if (mine.size() == 1) return mine.first();
        if (allAttrs.size() == 1) return allAttrs.first();
        return QString();
        };
    auto noteAttr = [&](const QString& owner, const QString& attr) {
        if (attr.isEmpty()) return;
        if (!allAttrs.contains(attr)) allAttrs << attr;
        QStringList& mine = ownerAttrs[owner];
        if (!mine.contains(attr)) mine << attr;
        };
    // A problem that states at least one quantity outright is SELF-CONTAINED:
    // everything it needs is in this input, and it must not read the variable
    // registry. Skipping that check let state leak between unrelated problems -
    // "Tom gives 2 apples to Sara" picked up a Sara left over from the previous
    // line and started her at 7 instead of 0. Only an input that states no
    // quantity of its own ("He eats 2 apples", a question on its own line) is
    // continuing an earlier one and may look back.
    bool selfContained = false;
    for (const Fact& f : facts)
        if (f.kind == Kind::Direct) { selfContained = true; break; }

    // Current value of a key: this run first, then - only when continuing an
    // earlier input - what that one bound. `found` distinguishes a real zero
    // from "never mentioned".
    auto currentValue = [&](const QString& key, bool& found) -> QString {
        if (values.contains(key)) { found = true; return values.value(key); }
        if (selfContained) { found = false; return QString(); }
        return remembered(key, found);
        };
    auto bind = [&](const QString& key, const QString& shownKey, const QString& value) {
        values[key] = value;
        // Register both spellings so "apple with timmy" and "apples with timmy"
        // both resolve in later expressions.
        MathEngine::setVariable(key, value);
        if (shownKey != key) MathEngine::setVariable(shownKey, value);
        };

    // "Each box has 6 apples" is a rate that needs a count of boxes, and the
    // count can be stated either side of it ("There are 4 boxes" may come
    // first or second). Collect them and apply after every other fact is in.
    QVector<Fact> eachFacts;

    for (const Fact& f : facts) {
        if (f.kind == Kind::Each) { eachFacts << f; continue; }

        const QString owner = resolveOwner(f.owner, QString());
        if (owner.isEmpty()) {
            sol.failure = QStringLiteral("I couldn't tell who \"%1\" is about.").arg(f.sentence);
            return sol;
        }
        noteOwner(owner);

        const QString attr = inferAttr(f.attribute, owner);
        if (attr.isEmpty()) {
            sol.failure = QStringLiteral("I couldn't tell what \"%1\" is counting.").arg(f.sentence);
            return sol;
        }
        noteAttr(owner, attr);

        // Canonical key for matching; the user's own wording for display. When
        // the noun was inferred rather than typed there is no user wording, so
        // both fall back to the canonical form.
        const QString key = keyFor(attr, owner);
        const QString shown = f.attribute.isEmpty() ? key
                                                    : writtenKeyFor(f.attribute, owner);
        // The user's own wording, for anything they will read. `attr` is stemmed
        // for matching, so putting it in a message gives "-28 apple".
        const QString attrSpoken = f.attribute.isEmpty() ? attr : norm(f.attribute);
        QString expr;      // parenthesised, for evaluation
        QString dispExpr;  // bare, for the working shown to the user

        if (f.kind == Kind::Direct) {
            expr = dispExpr = f.qty;
        }
        else if (f.kind == Kind::Gain || f.kind == Kind::Lose) {
            // A change needs something to change. Unlike a Direct fact, an
            // unknown starting point here is a real gap in the problem, not a
            // new binding - "He ate 3 apples" says nothing about how many he had.
            bool found = false;
            const QString base = currentValue(key, found);
            if (!found) {
                sol.failure = QStringLiteral(
                    "\"%1\" changes how many %2 %3 has, but I was never told the starting number.")
                    .arg(f.sentence, attr, owner);
                return sol;
            }
            const QChar op = (f.kind == Kind::Gain) ? '+' : '-';
            expr = QStringLiteral("(%1) %2 (%3)").arg(base, QString(op), f.qty);
            dispExpr = QStringLiteral("%1 %2 %3").arg(base, QString(op), f.qty);
        }
        else if (f.kind == Kind::Give) {
            const QString recipient = resolveOwner(f.other, owner);
            if (recipient.isEmpty()) {
                sol.failure = QStringLiteral("I couldn't tell who receives in \"%1\".").arg(f.sentence);
                return sol;
            }
            // The receiver may be new to the problem; starting them at zero is
            // the natural reading of "Tom gives 2 apples to Sara".
            const QString recipKey = keyFor(attr, recipient);
            bool haveRecip = false;
            const QString recipBase = currentValue(recipKey, haveRecip);
            const QString recipStart = haveRecip ? recipBase : QStringLiteral("0");

            bool okR = false;
            const QString recipVal =
                evalToString(QStringLiteral("(%1) + (%2)").arg(recipStart, f.qty), okR);
            if (!okR) {
                sol.failure = QStringLiteral("I couldn't work out a number from \"%1\".").arg(f.sentence);
                return sol;
            }

            // The giver's starting count is often simply not stated - "Sara
            // gives him 3 apples" tells you what Tom gained and nothing about
            // Sara. Track her only if we already know her count; refusing the
            // whole problem over an unstated number the question never asks
            // about would be pedantry, and inventing a zero for her would put a
            // negative in the working.
            bool haveGiver = false;
            const QString giverBase = currentValue(key, haveGiver);
            QString giverVal;
            if (haveGiver) {
                bool okG = false;
                giverVal = evalToString(QStringLiteral("(%1) - (%2)").arg(giverBase, f.qty), okG);
                if (!okG) {
                    sol.failure = QStringLiteral("I couldn't work out a number from \"%1\".").arg(f.sentence);
                    return sol;
                }
                if (isNegativeCount(giverVal)) {
                    sol.failure = QStringLiteral(
                        "\"%1\" would leave %2 with %3 %4 - more is given away than %2 has.")
                        .arg(f.sentence, owner, giverVal, attrSpoken);
                    return sol;
                }
            }

            noteOwner(recipient);
            // ...then put the SUBJECT back on top. A transfer mentions the
            // recipient, so they must be findable by a later "him"/"her" - but
            // they do not take over the sentence. In "Tom has 20 apples and
            // gives 5 to Sara and eats 3", the one eating is Tom; leaving Sara
            // as most-recent made the trailing clause subtract from her instead.
            noteOwner(owner);
            noteAttr(recipient, attr);
            if (haveGiver) bind(key, shown, giverVal);
            bind(recipKey, recipKey, recipVal);

            // One sentence, up to two changes - show each so the working stays
            // honest about what actually moved.
            if (haveGiver) {
                Step giveStep;
                giveStep.sentence = f.sentence;
                giveStep.binding = QStringLiteral("%1 = %2 - %3").arg(shown, giverBase, f.qty);
                giveStep.value = giverVal;
                sol.working << giveStep;

                Step recvStep;
                recvStep.sentence = QString();
                recvStep.binding = QStringLiteral("%1 = %2 + %3").arg(recipKey, recipStart, f.qty);
                recvStep.value = recipVal;
                sol.working << recvStep;
            }
            else {
                Step recvStep;
                recvStep.sentence = f.sentence;
                recvStep.binding = QStringLiteral("%1 = %2 + %3").arg(recipKey, recipStart, f.qty);
                recvStep.value = recipVal;
                sol.working << recvStep;
            }
            continue;
        }
        else {
            const QString otherOwner = resolveOwner(f.other, owner);
            const QString otherKey = keyFor(attr, otherOwner);
            // The owner being referred to may have been bound by an EARLIER
            // input ("timmy has five apples" on its own line, then "sarah has 3
            // more apples than timmy"). Consult memory before giving up.
            bool found = false;
            const QString base = currentValue(otherKey, found);
            if (!found) {
                sol.failure = QStringLiteral(
                    "\"%1\" refers to %2, but I don't know how many %3 %2 has yet.")
                    .arg(f.sentence, norm(otherOwner), attr);
                return sol;
            }
            values[otherKey] = base;
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
        if (isNegativeCount(value)) {
            sol.failure = QStringLiteral(
                "\"%1\" would leave %2 with %3 %4, and you can't have a negative "
                "number of things. Check the numbers.")
                .arg(f.sentence, owner, value, attrSpoken);
            return sol;
        }
        bind(key, shown, value);

        Step st;
        st.sentence = f.sentence;
        st.binding = QStringLiteral("%1 = %2").arg(shown, dispExpr);
        st.value = value;
        sol.working << st;
    }

    // -- Rates: "Each <container> has <N> <things>" ----------------------------
    // Applied last, so the count of containers can be stated before or after the
    // rate. Every owner holding containers gets the product: "Tom has 3 bags,
    // each bag has 4 apples" gives Tom 12, and "There are 4 boxes, each box has
    // 6 apples" gives the anonymous owner 24 - one rule covers both because the
    // container count is looked up the same way in each case.
    for (const Fact& f : eachFacts) {
        const QString container = canonAttribute(f.other);
        const QString attr = canonAttribute(f.attribute);
        if (container.isEmpty() || attr.isEmpty()) {
            sol.failure = QStringLiteral("I couldn't read \"%1\".").arg(f.sentence);
            return sol;
        }

        const QString containerPrefix = container + QStringLiteral(" with ");
        bool applied = false;
        // Snapshot the keys first: the loop binds new entries into `values`.
        const QStringList holders = values.keys();
        for (const QString& k : holders) {
            if (!k.startsWith(containerPrefix)) continue;
            const QString holder = k.mid(containerPrefix.size());
            const QString count = values.value(k);

            bool ok = false;
            const QString value =
                evalToString(QStringLiteral("(%1) * (%2)").arg(count, f.qty), ok);
            if (!ok) {
                sol.failure = QStringLiteral("I couldn't work out a number from \"%1\".").arg(f.sentence);
                return sol;
            }
            const QString key = keyFor(attr, holder);
            noteAttr(holder, attr);
            bind(key, key, value);

            Step st;
            st.sentence = f.sentence;
            st.binding = QStringLiteral("%1 = %2 * %3").arg(key, count, f.qty);
            st.value = value;
            sol.working << st;
            applied = true;
        }
        if (!applied) {
            // Echo the user's own words, not the canonical singular keys - the
            // stemmed form reads as broken English ("how many apple are in each
            // box") in the one message they see when something went wrong.
            sol.failure = QStringLiteral(
                "\"%1\" tells me what is in each %2, but not how many %2 there are.")
                .arg(f.sentence, norm(f.other));
            return sol;
        }
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
    QString attr = canonAttribute(query.attribute);
    QString attrShown = norm(query.attribute);

    // The noun can be left out entirely - "How many do they have together?",
    // "How many does Tom have?". Infer it when the problem only ever talks about
    // one kind of thing, which is the case for almost every problem this solver
    // is aimed at. With two or more we refuse and say which, rather than picking
    // one and being confidently wrong.
    if (attr.isEmpty()) {
        QStringList candidates;
        if (fromMemory) {
            for (const QString& k : MathEngine::definedVariableNames()) {
                const int at = k.indexOf(QStringLiteral(" with "));
                if (at <= 0) continue;
                const QString a = k.left(at);
                if (canonAttribute(a) == a && !candidates.contains(a)) candidates << a;
            }
        }
        else {
            for (const Fact& f : facts) {
                const QString a = canonAttribute(f.attribute);
                if (!a.isEmpty() && !candidates.contains(a)) candidates << a;
            }
        }
        if (candidates.size() == 1) {
            attr = attrShown = candidates.first();
        }
        else if (candidates.isEmpty()) {
            sol.failure = QStringLiteral("You didn't say what to count.");
            return sol;
        }
        else {
            std::sort(candidates.begin(), candidates.end());
            sol.failure = QStringLiteral("Which do you mean - %1?")
                .arg(candidates.join(QStringLiteral(" or ")));
            return sol;
        }
    }

    bool ok = false;

    if (query.kind == QKind::Lookup) {
        // Resolve through the same pronoun table the facts used, so
        // "... How many does he have?" works.
        const QString qOwner = resolveOwner(query.owner, QString());
        if (qOwner.isEmpty()) {
            sol.failure = QStringLiteral("I couldn't tell who \"%1\" is asking about.")
                .arg(sol.question);
            return sol;
        }
        const QString key = keyFor(attr, qOwner);
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
        // Echo the user's own wording, not the canonical key. `key` is stemmed
        // for matching ("apples" -> "apple", "people" -> "person"), which is
        // correct internally and reads as a typo when shown back to them.
        sol.detail = QStringLiteral("%1 with %2").arg(attrShown, qOwner);
    }
    else if (query.kind == QKind::Difference) {
        const QString ownerA = resolveOwner(query.owner, QString());
        const QString ownerB = resolveOwner(query.other, ownerA);
        const QString a = keyFor(attr, ownerA);
        const QString b = keyFor(attr, ownerB);
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
            // Sum the FINAL state, not the facts. Iterating facts double-counted
            // any owner named more than once - "Tom has 5 apples. He eats 2."
            // is two facts about one person, and adding both gave 5 + 3 = 8.
            // `values` holds one canonical entry per owner+attribute, already
            // pronoun-resolved and with every gain/loss/transfer applied.
            const QString prefix = attr + QStringLiteral(" with ");
            for (auto it = values.constBegin(); it != values.constEnd(); ++it)
                if (it.key().startsWith(prefix))
                    parts << it.value();
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
        // The second half of a transfer has no sentence of its own - one
        // sentence moved a quantity between two people. Indent it under the
        // first rather than repeating the sentence or leaving a ragged "  ->".
        if (st.sentence.isEmpty()) {
            lines << QStringLiteral("    ...  ->  %1 = %2").arg(st.binding, st.value);
            continue;
        }
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
