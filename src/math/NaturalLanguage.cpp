#include "NaturalLanguage.h"
#include <QRegularExpression>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <boost/multiprecision/cpp_int.hpp>
#include <algorithm>

using boost::multiprecision::cpp_int;

static bool isNumericToken(const QString& s) {
    static const QRegularExpression re(R"(^\d+(?:\.\d+)?$)");
    return re.match(s).hasMatch();
}

// -- Helpers -------------------------------------------------------------------
static cpp_int pow10(int exp)
{
    cpp_int r = 1;
    while (exp--) r *= 10;
    return r;
}

static QString makeConwayGuyName(int zeros)
{
    if (zeros < 0)  throw std::invalid_argument("zeros must be >= 0");
    if (zeros < 3)  return {};
    if (zeros < 6)  return QStringLiteral("thousand");
    if (zeros < 9)  return QStringLiteral("million");
    if (zeros < 12) return QStringLiteral("billion");
    if (zeros < 15) return QStringLiteral("trillion");
    if (zeros < 18) return QStringLiteral("quadrillion");
    if (zeros < 21) return QStringLiteral("quintillion");
    if (zeros < 24) return QStringLiteral("sextillion");
    if (zeros < 27) return QStringLiteral("septillion");
    if (zeros < 30) return QStringLiteral("octillion");
    if (zeros < 33) return QStringLiteral("nonillion");

    static const QString names[4][10] = {
        { "", "un", "do", "tre", "quattuor", "quin", "se", "septe", "octo", "nove" },
        { "", "deci", "viginti", "triginta", "quadraginta",
          "quinquaginta", "sexaginta", "septuaginta", "octoginta", "nonaginta" },
        { "", "centi", "ducenti", "trecenti", "quadringenti",
          "quingenti", "sescenti", "septingenti", "octingenti", "nongenti" },
        { "", "milli", "billi", "trilli", "quadrilli",
          "quintilli", "sextilli", "septilli", "octilli", "nonilli" }
    };
    static const QStringList rules[4][10] = {
        { {}, {}, {}, {"s"}, {}, {}, {"s","x"}, {"m","n"}, {}, {"m","n"} },
        { {}, {"n"}, {"m","s"}, {"n","s"}, {"n","s"},
          {"n","s"}, {"n"}, {"n"}, {"m","x"}, {} },
        { {}, {"n","x"}, {"n"}, {"n","s"}, {"n","s"},
          {"n","s"}, {"n"}, {"n"}, {"m","x"}, {} },
        { {}, {}, {}, {}, {}, {}, {}, {}, {}, {} }
    };

    int group = (zeros - 3) / 3;
    QVector<int> digits;
    while (group > 0) { digits.push_back(group % 10); group /= 10; }

    QVector<QString> out(digits.size());
    for (int i = digits.size() - 1; i >= 0; --i) {
        if (i >= 4) throw std::runtime_error("Number too large");
        out[i] = names[i][digits[i]];
        if (i == 0) {
            const QStringList& lastRules = rules[1][digits.back()];
            const QStringList& myRules = rules[0][digits[0]];
            for (const QString& r : myRules)
                if (lastRules.contains(r)) out[0] += r;
        }
    }

    QString result;
    for (const QString& s : out) result += s;
    if (result.endsWith('a')) { result.chop(1); result += 'i'; }
    if (!result.isEmpty()) result[0] = result[0].toUpper();
    return result + QStringLiteral("llion");
}

// Built once at startup, reused on every preprocess() call
static const QMap<QString, cpp_int>& illionMap()
{
    static const QMap<QString, cpp_int> map = []() {
        QMap<QString, cpp_int> m;
        for (int zeros = 6; zeros <= NaturalLanguage::MAX_ZEROS; zeros += 3)
            m.insert(makeConwayGuyName(zeros).toLower(), pow10(zeros));
        return m;
        }();
    return map;
}

// -- Number-word expander ------------------------------------------------------
static QString expandNumberWords(const QString& input)
{
    static const QMap<QString, cpp_int> wordMap = {
        // Cardinals
        {"zero",0},{"one",1},{"two",2},{"three",3},{"four",4},
        {"five",5},{"six",6},{"seven",7},{"eight",8},{"nine",9},
        {"ten",10},{"eleven",11},{"twelve",12},{"thirteen",13},
        {"fourteen",14},{"fifteen",15},{"sixteen",16},{"seventeen",17},
        {"eighteen",18},{"nineteen",19},
        {"twenty",20},{"thirty",30},{"forty",40},{"fifty",50},
        {"sixty",60},{"seventy",70},{"eighty",80},{"ninety",90},

        // Ordinals
        {"first",1},{"second",2},{"third",3},{"fourth",4},
        {"fifth",5},{"sixth",6},{"seventh",7},{"eighth",8},
        {"ninth",9},{"tenth",10},{"eleventh",11},{"twelfth",12},
        {"thirteenth",13},{"fourteenth",14},{"fifteenth",15},
        {"sixteenth",16},{"seventeenth",17},{"eighteenth",18},
        {"nineteenth",19},
        {"twentieth",20},{"thirtieth",30},{"fortieth",40},
        {"fiftieth",50},{"sixtieth",60},{"seventieth",70},
        {"eightieth",80},{"ninetieth",90},

        {"hundred",pow10(2)},{"thousand",pow10(3)},{"million",pow10(6)},
        {"billion",pow10(9)},{"trillion",pow10(12)},{"quadrillion",pow10(15)},
        {"quintillion",pow10(18)},{"sextillion",pow10(21)},{"septillion",pow10(24)},
        {"octillion",pow10(27)},{"nonillion",pow10(30)},{"decillion",pow10(33)},
        {"undecillion",pow10(36)},{"duodecillion",pow10(39)},
        {"vigintillion",pow10(63)},{"trigintillion",pow10(93)},
        {"quadragintillion",pow10(123)},{"quinquagintillion",pow10(153)},
        {"sexagintillion",pow10(183)},{"septuagintillion",pow10(213)},
        {"octogintillion",pow10(243)},{"nonagintillion",pow10(273)},
        {"centillion",pow10(303)},{"ducentillion",pow10(603)},
        {"trecentillion",pow10(903)},{"quadringentillion",pow10(1203)},
        {"quingentillion",pow10(1503)},{"sescentillion",pow10(1803)},
        {"septingentillion",pow10(2103)},{"octingentillion",pow10(2403)},
        {"nongentillion",pow10(2703)},{"millinillion",pow10(3003)}
    };
    static const QSet<QString> ignoreWords = { "and", "the", "a", "an"};

    static const QMap<QString, cpp_int> fullMap = []() {
        QMap<QString, cpp_int> m = wordMap;
        const auto& im = illionMap();
        for (auto it = im.cbegin(); it != im.cend(); ++it)
            m.insert(it.key(), it.value());
        return m;
        }();

    static QRegularExpression hyphenCompoundRe(
        R"(\b(twenty|thirty|forty|fifty|sixty|seventy|eighty|ninety)-([a-zA-Z]+)\b)",
        QRegularExpression::CaseInsensitiveOption);
    QString normalised = input;
    normalised.replace(hyphenCompoundRe, "\\1 \\2");

    // Tokenize into: numbers, words, whitespace runs, or single other chars.
    static const QRegularExpression tokenRe(R"(\d+(?:\.\d+)?|[A-Za-z]+|\s+|.)");
    QStringList tokens;
    {
        auto it = tokenRe.globalMatch(normalised);
        while (it.hasNext())
            tokens.append(it.next().captured(0));
    }

    QString output;
    int i = 0;
    while (i < tokens.size()) {
        const QString& token = tokens[i];
        const QString   lower = token.toLower();
        // Only start if the token is a number‑word, not a numeric literal
        const bool isStart = fullMap.contains(lower);

        if (!isStart) {
            output += token;
            ++i;
            continue;
        }

        cpp_int total = 0, current = 0;
        int j = i;

        // Greedily consume a run of number‑words, allowing whitespace and
        // ignored words between them, but break if we encounter a numeric literal
        // (which is not a number‑word).
        while (true) {
            int k = j;
            while (k < tokens.size() &&
                (tokens[k].trimmed().isEmpty() ||
                    ignoreWords.contains(tokens[k].toLower())))
                ++k;
            if (k >= tokens.size()) break;

            const QString& w = tokens[k];
            const QString  wl = w.toLower();

            // If it's a numeric literal (e.g., "0.25"), break - we don't consume it.
            if (isNumericToken(w))
                break;

            if (!fullMap.contains(wl))
                break; // not a number‑word, stop consuming

            cpp_int n = fullMap[wl];

            if (n == 100) {
                if (current == 0) current = 1;
                current *= 100;
            }
            else if (n >= 1000) {
                if (current == 0) current = 1;
                total += current * n;
                current = 0;
            }
            else {
                current += n;
            }
            j = k + 1;
        }
        if (current > 0) total += current;
        output += QString::fromStdString(total.str());
        i = j;
    }
    return output;
}

// -- Thousands-separator stripper ----------------------------------------------
static QString normalizeNumericFormatting(QString input)
{
    static QRegularExpression thousandsSep(R"((\d),(?=\d{3}(\D|$)))");
    input.replace(thousandsSep, "\\1");
    return input;
}

// -- Question-prefix stripper --------------------------------------------------
QString stripQuestionPrefix(const QString& input)
{
    static const QList<QString> phrases = []() {
        QList<QString> p = {
            // "Can/could you" wrappers
            "Could you please calculate", "Could you please tell me",
            "Can you please calculate",   "Can you please tell me",
            "Could you calculate", "Could you tell me", "Could you find",
            "Can you calculate",   "Can you tell me",   "Can you find",
            "Could you", "Can you",

            // Command style
            "Give me", "Tell me", "Show me",

            // Calculate verbs
            "Calculate", "Compute", "Work out", "Figure out", "Find", "Solve",

            // Question family
            "How much would", "How much is", "How much", "How many",
            "What's", "Whats", "What is", "What was", "What are", "What",

            // Casual variants
            "Wuts", "Wats", "Whts", "Wut", "Wat",

            // Result style
            "Result of", "Answer to", "Answer",
            "calculate", "compute", "evaluate", "find",

            "mathematically"
        };
        std::sort(p.begin(), p.end(),
            [](const QString& a, const QString& b) { return a.length() > b.length(); });
        return p;
        }();


    QString trimmed = input.trimmed();

    bool matched;
    do {
        matched = false;

        for (const QString& phrase : phrases) {
            if (!trimmed.startsWith(phrase, Qt::CaseInsensitive))
                continue;
            if (trimmed.length() > phrase.length()) {
                QChar after = trimmed[phrase.length()];
                // Don't strip if the phrase is only a prefix of a longer word
                // ("What" in "Whatever") or the start of a function call.
                if (after.isLetter() || after == '(')
                    continue;
            }
            trimmed = trimmed.mid(phrase.length()).trimmed();
            matched = true;
            break;
        }

        // Strip filler words after every successful prefix removal
        bool removedFiller;
        do {
            removedFiller = false;
            static const QStringList filler = {
                "the", "a", "an", "please", "kindly", "just", "to"
            };
            for (const QString& word : filler) {
                if (trimmed.startsWith(word + " ", Qt::CaseInsensitive)) {
                    // Don't strip an article that's really a variable/operand,
                    // e.g. the "a" in "a + b = 10" or "a = 5" (followed by an
                    // operator, not a noun).
                    const QString rest = trimmed.mid(word.length()).trimmed();
                    if (!rest.isEmpty() && QStringLiteral("+-*/^=%()<>").contains(rest[0]))
                        continue;
                    trimmed = rest;
                    removedFiller = true;
                    break;
                }
            }
        } while (removedFiller);

    } while (matched);

    // Remove trailing punctuation - but keep a factorial "!" (one that follows a
    // number or ')'), so "5!" isn't mistaken for an exclamation and gutted to "5".
    while (!trimmed.isEmpty()) {
        const QChar last = trimmed.back();
        if (last == '!') {
            const QChar prev = trimmed.size() >= 2 ? trimmed[trimmed.size() - 2] : QChar();
            if (prev.isDigit() || prev == ')') break;   // factorial, not punctuation
            trimmed.chop(1);
        }
        else if (last == '?' || last == '.') {
            trimmed.chop(1);
        }
        else break;
        trimmed = trimmed.trimmed();
    }

    // Remove trailing filler words
    static const QStringList endings = { " is", " was", " are", " please", " kindly" };
    bool removedEnding;
    do {
        removedEnding = false;
        for (const QString& end : endings) {
            if (trimmed.endsWith(end, Qt::CaseInsensitive)) {
                trimmed.chop(end.length());
                trimmed = trimmed.trimmed();
                removedEnding = true;
                break;
            }
        }
    } while (removedEnding);

    return trimmed;
}

// -- "itself" expander ---------------------------------------------------------
// FIX 1: expandItself now runs AFTER all "itself N times" patterns so those
// patterns get first pick before "itself" is substituted away.
static QString expandItself(QString text)
{
    static const QRegularExpression itselfRe(R"(\bitself\b)",
        QRegularExpression::CaseInsensitiveOption);
    int pos = 0;
    while ((pos = text.indexOf(itselfRe, pos)) != -1)
    {
        int end = pos - 1;
        while (end >= 0 && text[end].isSpace()) --end;

        while (end >= 0) {
            const QChar c = text[end];
            const bool isSym = QString("+-*/^=×÷").contains(c);
            const bool isXOp = (c == 'x' || c == 'X')
                && (end == 0 || !text[end - 1].isLetterOrNumber())
                && (end + 1 >= text.size() || !text[end + 1].isLetterOrNumber());
            if (!isSym && !isXOp) break;
            --end;
            while (end >= 0 && text[end].isSpace()) --end;
        }

        if (end < 0) { pos += 6; continue; }

        int start = end;

        if (text[end] == ')') {
            int depth = 1;
            --start;
            while (start >= 0 && depth > 0) {
                if (text[start] == ')') ++depth;
                else if (text[start] == '(') --depth;
                --start;
            }
            ++start;
            // Include function name before '('
            while (start > 0 &&
                (text[start - 1].isLetterOrNumber() || text[start - 1] == '_'))
                --start;
        }
        else {
            while (start >= 0 &&
                (text[start].isLetterOrNumber() ||
                    text[start] == '_' || text[start] == '.'))
                --start;
            ++start;
        }

        QString operand = text.mid(start, end - start + 1);
        text.replace(pos, 6, operand);
        pos += operand.length();
    }
    return text;
}

static QString replaceSuperScripts(const QString& input)
{

    // Map each superscript digit to its normal ASCII equivalent
    static const QMap<QChar, QChar> supMap = {
    {QChar(u'⁰'), QChar(u'0')},
    {QChar(u'¹'), QChar(u'1')},
    {QChar(u'²'), QChar(u'2')},
    {QChar(u'³'), QChar(u'3')},
    {QChar(u'⁴'), QChar(u'4')},
    {QChar(u'⁵'), QChar(u'5')},
    {QChar(u'⁶'), QChar(u'6')},
    {QChar(u'⁷'), QChar(u'7')},
    {QChar(u'⁸'), QChar(u'8')},
    {QChar(u'⁹'), QChar(u'9')}
    };

    // Build a character class containing all superscript digits
    QString supChars;
    for (auto it = supMap.begin(); it != supMap.end(); ++it)
        supChars += it.key();

    static QRegularExpression supRe(QStringLiteral("[") + supChars + QStringLiteral("]+"));

    QString result = input;
    int pos = 0;
    while (true) {
        QRegularExpressionMatch match = supRe.match(result, pos);
        if (!match.hasMatch())
            break;

        int start = match.capturedStart();
        int end = match.capturedEnd();
        QString captured = match.captured(0);

        // Convert each superscript char to its digit
        QString digits;
        for (const QChar& ch : captured)
            digits += supMap[ch];

        // Replace the whole run with "^" + digits (e.g., "²³" → "^23")
        result.replace(start, end - start, QStringLiteral("^") + digits);

        // Move past the inserted replacement
        pos = start + 1 + digits.length();
    }
    return result;
}
// -- Equality-question normalization -------------------------------------------
// Rewrites natural-language "is X equal to Y?" style questions into the plain
// "X = Y" form that MathEngine::tryAlgebra's equality-verification path (and its
// equation solver, when variables are present) understands. Examples:
//
//   "is 253 + 342 equal to 595"   -> "253 + 342 = 595"
//   "does 253 + 342 equal 600"    -> "253 + 342 = 600"
//   "253 + 342 equals 595"        -> "253 + 342 = 595"
//   "is 10*10 the same as 100"    -> "10*10 = 100"
//   "is 2x + 1 equal to 7"        -> "2x + 1 = 7"   (then solved for x)
//
// Input is already lower-cased and had its trailing '?' stripped by the time
// this runs (see preprocess()).
static QString normalizeEqualityQuestions(QString text)
{
    // 1) Turn equality connectives into '='. Ordered most-specific first so that
    //    e.g. "equal to" is consumed before the bare-"equal" fallback, and
    //    "is/are equal to" before "equal to". Each is a whole-word match so we
    //    never touch substrings ("equally", "equator", ...).
    static const std::vector<QRegularExpression> connectives = []() {
        static const char* pats[] = {
            R"(\b(?:is|are)\s+equal\s+to\b)",
            R"(\bequal\s+to\b)",
            R"(\bequals\b)",
            R"(\bis\s+the\s+same\s+as\b)",
            R"(\b(?:the\s+)?same\s+as\b)",
            R"(\bequal\b)",   // bare verb: "does X equal Y"
        };
        std::vector<QRegularExpression> v;
        v.reserve(std::size(pats));
        for (const char* p : pats)
            v.emplace_back(QString::fromLatin1(p),
                QRegularExpression::CaseInsensitiveOption);
        return v;
    }();
    for (const QRegularExpression& re : connectives)
        text.replace(re, QStringLiteral("="));

    // 2) Only if that actually produced an equality, drop a leading yes/no
    //    interrogative ("is/are/does/do/did/was/were 253 + 342 = 595" ->
    //    "253 + 342 = 595"). Gating on the '=' keeps us from stripping a leading
    //    word from anything that isn't an equality question.
    if (text.contains('=')) {
        static const QRegularExpression leadRe(
            R"(^\s*(?:is|are|does|do|did|was|were)\b\s*)",
            QRegularExpression::CaseInsensitiveOption);
        text.replace(leadRe, QString());
    }
    return text;
}

// -- Verbose variable-definition normaliser ------------------------------------
// "Define a variable called x and assign the value 90 to it", "Define x = 90",
// "Set x to 90", "Assign 90 to x", "Create a variable named y with value 42" ...
// -> "x = 90". Two passes: locate the name, then the value. Input is already
// lower-cased. Only engages on a leading definition/assignment verb.
static QString normalizeDefinition(const QString& s)
{
    static const QRegularExpression verbRe(
        R"(^\s*(?:define|declare|create|make|assign|set)\b)");
    if (!verbRe.match(s).hasMatch()) return s;

    // Value-first form: "assign [the value] N to [the variable] X".
    static const QRegularExpression assignToRe(
        R"(^\s*assign\s+(?:the\s+value\s+)?(.+?)\s+to\s+(?:the\s+variable\s+)?["']?([a-z][a-z0-9_]*)["']?\s*$)");
    if (const auto m = assignToRe.match(s); m.hasMatch())
        return m.captured(2) + " = " + m.captured(1).trimmed();

    // Otherwise: extract the variable name...
    QString name;
    static const QRegularExpression nameCalledRe(
        R"(\b(?:variable|var)\s+(?:called|named)\s*["']?([a-z][a-z0-9_]*)["']?)");
    static const QRegularExpression nameVarRe(
        R"(\b(?:variable|var)\s+["']?([a-z][a-z0-9_]*)["']?)");
    static const QRegularExpression nameVerbRe(
        R"(^\s*(?:define|declare|create|make|set)\s+["']?([a-z][a-z0-9_]*)["']?)");
    if (const auto m = nameCalledRe.match(s); m.hasMatch()) name = m.captured(1);
    else if (const auto m = nameVarRe.match(s); m.hasMatch()) name = m.captured(1);
    else if (const auto m = nameVerbRe.match(s); m.hasMatch()) name = m.captured(1);
    if (name.isEmpty()) return s;

    // ...then the value, after a binder phrase (=, to, as, be, "equal to",
    // "value", "with value", ...), trimming a trailing "to it".
    static const QRegularExpression valRe(
        R"((?:equal\s+to|to\s+be|(?:with\s+)?(?:the\s+)?value(?:\s+of)?|equals?|=|as|to|be)\s+(.+?)(?:\s+to\s+it)?\s*$)");
    const auto vm = valRe.match(s);
    if (!vm.hasMatch()) return s;
    const QString value = vm.captured(1).trimmed();
    if (value.isEmpty()) return s;

    return name + " = " + value;
}

// -- Main entry point ----------------------------------------------------------
QString NaturalLanguage::preprocess(const QString& input)
{
    QString result = input.toLower();

    // FIX 6: strip ">>" immediately, before any other pass touches the string.
    static QRegularExpression promptRe(R"(^\s*>>\s*)");
    result.replace(promptRe, "");

    result = stripQuestionPrefix(result);

    // "let x be equal to 5" / "let x be 5" / "let x = 5" -> "x = 5": a natural way
    // to define a variable. The value ("5" / "five") is normalised further below,
    // and the resulting "x = ..." is stored by the assignment handler.
    static const QRegularExpression letWordRe(
        R"(^\s*let\s+([a-z][a-z0-9_]*)\s+(?:be\s+equal\s+to|be\s+equals?|equal\s+to|equals?|be)\s+)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(letWordRe, "\\1 = ");
    static const QRegularExpression letEqRe(
        R"(^\s*let\s+([a-z][a-z0-9_]*)\s*=\s*)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(letEqRe, "\\1 = ");

    // "Define x = 90", "Set x to 90", "Assign 90 to x", and the verbose
    // "Define a variable called x and assign the value 90 to it".
    result = normalizeDefinition(result);

    result = replaceSuperScripts(result);


  // Multi-word and hyphenated forms must be caught as a unit BEFORE "one"/
  // "two"/"three" get consumed by the number-word scanner.
    static const std::vector<std::pair<QRegularExpression, QString>> earlyFractionPatterns = []()
        {
            struct Def { const char* pat; const char* repl; };
            static const Def defs[] = {
                { R"(\bthree[ -]quarters?\b)",  "(3/4)"  },
                { R"(\btwo[ -]thirds?\b)",      "(2/3)"  },
                { R"(\bone[ -]quarters?\b)",    "(1/4)"  },
                { R"(\bone[ -]thirds?\b)",      "(1/3)"  },
                { R"(\bone[ -]fourths?\b)",     "(1/4)"  },
                { R"(\bone[ -]eighths?\b)",     "(1/8)"  },
                { R"(\bone[ -]sixths?\b)",      "(1/6)"  },
                { R"(\bone[ -]fifths?\b)",      "(1/5)"  },
                { R"(\bone[ -]sevenths?\b)",    "(1/7)"  },
                { R"(\bone[ -]ninths?\b)",      "(1/9)"  },
                { R"(\bone[ -]tenths?\b)",      "(1/10)" },
                { R"(\ba[ -]quarters?\b)",      "(1/4)"  },
                { R"(\ba[ -]hal(?:f|ves)\b)",   "(1/2)"  },
            };
            std::vector<std::pair<QRegularExpression, QString>> v;
            v.reserve(std::size(defs));
            for (const auto& d : defs)
                v.emplace_back(
                    QRegularExpression(QString::fromLatin1(d.pat),
                        QRegularExpression::CaseInsensitiveOption),
                    QString::fromLatin1(d.repl));
            return v;
        }();

    for (const auto& [re, repl] : earlyFractionPatterns)
        result.replace(re, repl);

    result = expandNumberWords(result);
    // Single-word forms are safe here because ordinal words like "fourth",
    // "third" etc. are already consumed and converted to digits by this point.
    static const std::vector<std::pair<QRegularExpression, QString>> lateFractionPatterns = []()
        {
            struct Def { const char* pat; const char* repl; };
            static const Def defs[] = {
                { R"(\bquarters?\b)",   "(1/4)"  },
                { R"(\bhal(?:f|ves)\b)","(1/2)"  },
                { R"(\bthirds?\b)",     "(1/3)"  },
                { R"(\bfourths?\b)",    "(1/4)"  },
                { R"(\beighths?\b)",    "(1/8)"  },
                { R"(\bsixths?\b)",     "(1/6)"  },
                { R"(\bfifths?\b)",     "(1/5)"  },
                { R"(\bsevenths?\b)",   "(1/7)"  },
                { R"(\bninths?\b)",     "(1/9)"  },
                { R"(\btenths?\b)",     "(1/10)" },
            };
            std::vector<std::pair<QRegularExpression, QString>> v;
            v.reserve(std::size(defs));
            for (const auto& d : defs)
                v.emplace_back(
                    QRegularExpression(QString::fromLatin1(d.pat),
                        QRegularExpression::CaseInsensitiveOption),
                    QString::fromLatin1(d.repl));
            return v;
        }();

    for (const auto& [re, repl] : lateFractionPatterns)
        result.replace(re, repl);
    result = normalizeNumericFormatting(result);

    // Strip ordinal suffixes from bare digit ordinals (e.g. "1st" → "1")
    static QRegularExpression ordinalSuffixRe(
        R"(\b(\d+)(st|nd|rd|th)\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(ordinalSuffixRe, "\\1");

    // ------------------------------------------------------------------
    // Normalize Unicode math symbols
    // ------------------------------------------------------------------
    result.replace(QChar(0x00D7), " * "); // ×
    result.replace(QChar(0x2715), " * "); // ✕
    result.replace(QChar(0x00F7), " / "); // ÷
    result.replace(QChar(0x2212), "-");   // −

    // ------------------------------------------------------------------
    // "X squared" / "X cubed"
    // ------------------------------------------------------------------
    static QRegularExpression squaredRe(
        R"((\([^()]+\)|[^\s]+)\s+squared\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(squaredRe, "(\\1^2)");

    static QRegularExpression cubedRe(
        R"((\([^()]+\)|[^\s]+)\s+cubed\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(cubedRe, "(\\1^3)");

    // ------------------------------------------------------------------
    // "X into itself"  (division - handled before generic "into")
    // ------------------------------------------------------------------
    static QRegularExpression intoItselfRe(
        R"((\([^()]+\)|[^\s]+)\s+into\s+itself\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(intoItselfRe, "(\\1 / \\1)");

    // ------------------------------------------------------------------
    // Percent
    // ------------------------------------------------------------------
    static QRegularExpression percentWordRe(R"((\d+(?:\.\d+)?)\s+percent\b)");
    result.replace(percentWordRe, "\\1%");

    // Contextual percent: must run before any generic % → /100 conversion.
    static QRegularExpression plusPercentRe(
        R"((\d+(?:\.\d+)?)\s*(?:plus|\+)\s*(\d+(?:\.\d+)?)%)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(plusPercentRe, "(\\1 + (\\2/100)*\\1)");

    static QRegularExpression minusPercentRe(
        R"((\d+(?:\.\d+)?)\s*(?:minus|-)\s*(\d+(?:\.\d+)?)%)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(minusPercentRe, "(\\1 - (\\2/100)*\\1)");



    static const QRegularExpression dozenRe(R"(\bdozens?\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(dozenRe, "(12)");
    // Natural-language equality questions ("is 253 + 342 equal to 595?",
    // "does X equal Y", "X equals Y", "X the same as Y") -> "X = Y". Supersedes
    // the old plain "is equal to" -> "=" replacement.
    result = normalizeEqualityQuestions(result);
    result.replace("to the power of", "^");
    result.replace("cuberoot", "cbrt");
    result.replace("squareroot", "sqrt");

  

    // "15 multiplied by/with itself 3 times" → (15^3)
    static QRegularExpression byItselfNTimesRe(
        R"((\([^()]+\)|[^\s]+)\s+multiplied\s+(?:by|with)\s+itself\s+(\d+)\s+times?\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(byItselfNTimesRe, "(\\1^\\2)");

    // "15 x/* itself 3 times" → (15^3)
    static QRegularExpression itselfTimesRe(
        R"((\([^()]+\)|[^\s]+)\s+(?:multiplied\s+(?:by|with)|x|\*)\s+itself\s+(\d+)\s+times?\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(itselfTimesRe, "(\\1^\\2)");

    // "15 multiplied by/with itself" (no count) → (15^2)
    static QRegularExpression byItselfRe(
        R"((\([^()]+\)|[^\s]+)\s+multiplied\s+(?:by|with)\s+itself\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(byItselfRe, "(\\1^2)");

    // FIX 1: expandItself runs HERE - after all "itself N times" patterns.
    result = expandItself(result);
    // ------------------------------------------------------------------
    // Exponent phrasing - raised patterns MUST come first
    // ------------------------------------------------------------------
    static QRegularExpression raiseRe(
        R"(\braise\s+(\([^()]+\)|[^\s]+)\s+to\b)",
        QRegularExpression::CaseInsensitiveOption);

    result.replace(raiseRe, "\\1 raised to");

    // "15 raised to the 5th [power]"
    static QRegularExpression raisedToRe(
        R"((\([^()]+\)|[^\s]+)\s+raised\s+to\s+(?:the\s+)?(\d+)(?:st|nd|rd|th)?(?:\s+power)?)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(raisedToRe, "(\\1^\\2)");

    // "15 raised to the 5th power"
    static QRegularExpression raisedToThePowerRe(
        R"((\([^()]+\)|[^\s]+)\s+raised\s+to\s+the\s+(\d+)(?:st|nd|rd|th)?(?:\s+power)?\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(raisedToThePowerRe, "(\\1^\\2)");

    // "15 raised to power [of] 5"
    static QRegularExpression raisedPowerRe(
        R"((\([^()]+\)|[^\s]+)\s+raised\s+to\s+(?:the\s+)?power(?:\s+of)?\s+(\d+)\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(raisedPowerRe, "(\\1^\\2)");

    // Now the plain "to" patterns (these must come after raised ones)
    static QRegularExpression toTheRe(
        R"((\([^()]+\)|[^\s]+)\s+to\s+the\s+(\d+)(?:st|nd|rd|th)?\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(toTheRe, "(\\1^\\2)");

    static QRegularExpression toThePowerRe(
        R"((\([^()]+\)|[^\s]+)\s+to\s+the(?:\s+power)?(?:\s+of)?\s+(\d+)\s*(?:st|nd|rd|th)?\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(toThePowerRe, "(\\1^\\2)");

    static QRegularExpression toPowerRe(
        R"((\([^()]+\)|[^\s]+)\s+to\s+(?:the\s+)?power(?:\s+of)?\s+(\d+)\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(toPowerRe, "(\\1^\\2)");

    // Clean stray "power" left after a parenthesis group
    static QRegularExpression trailingPowerRe(
        R"(\)\s+power\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(trailingPowerRe, ")");

    result = result.simplified();


    // ------------------------------------------------------------------
    // Multi-word operator phrases
    // These must run BEFORE the single-word fixpoint loop so "divided"
    // and "multiplied" don't leave orphaned words behind when "by" fires.
    // ------------------------------------------------------------------

    // FIX 4: "minus" operator word - was completely missing.
    static QRegularExpression minusRe(
        R"((\([^()]+\)|[^\s]+)\s+minus\s+(\([^()]+\)|[^\s]+))",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(minusRe, "(\\1 - \\2)");

    // "X divided by Y"
    static QRegularExpression dividedByRe(
        R"((\([^()]+\)|[^\s]+)\s+divided\s+by\s+(\([^()]+\)|[^\s]+))",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(dividedByRe, "(\\1 / \\2)");

    // "X multiplied by Y"
    static QRegularExpression multipliedByRe(
        R"((\([^()]+\)|[^\s]+)\s+multiplied\s+by\s+(\([^()]+\)|[^\s]+))",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(multipliedByRe, "(\\1 * \\2)");

    // ------------------------------------------------------------------
    // Root phrasing
    // FIX 5: one canonical set of root patterns - removed the duplicate
    // sqrtRe/cbrtRe that appeared earlier in the original file.
    // ------------------------------------------------------------------
    static const std::vector<std::pair<QRegularExpression, QString>> rootPatterns = {
        {
            QRegularExpression(
                R"(\b(?:the\s+)?square\s+root\s+of\s+(\([^()]+\)|[^\s]+))",
                QRegularExpression::CaseInsensitiveOption),
            "sqrt(\\1)"
        },
        {
            QRegularExpression(
                R"(\b(?:the\s+)?sqrt\s+of\s+(\([^()]+\)|[^\s]+))",
                QRegularExpression::CaseInsensitiveOption),
            "sqrt(\\1)"
        },
        {
            QRegularExpression(
                R"(\b(?:the\s+)?cube\s+root\s+of\s+(\([^()]+\)|[^\s]+))",
                QRegularExpression::CaseInsensitiveOption),
            "cbrt(\\1)"
        },
        {
            QRegularExpression(
                R"(\b(?:the\s+)?cbrt\s+of\s+(\([^()]+\)|[^\s]+))",
                QRegularExpression::CaseInsensitiveOption),
            "cbrt(\\1)"
        }
    };
    for (const auto& [re, repl] : rootPatterns)
        result.replace(re, repl);

    

    // ------------------------------------------------------------------
    // Mach number normalisation
    // ------------------------------------------------------------------
    static QRegularExpression machRe(R"(\bmach\s+(\d+(?:\.\d+)?)\b)",
        QRegularExpression::CaseInsensitiveOption);
    result.replace(machRe, "\\1 mach");

    // ------------------------------------------------------------------
    // Single-word operator fixpoint loop
    // FIX 7: "into" now maps to multiplication (*) not division.
    //        In everyday use (and especially in Indian English) "6 into 5"
    //        means 6 × 5, not 30 ÷ 6.  The "X goes into Y" division sense
    //        is already handled above by "divided by".
    // ------------------------------------------------------------------
    static QRegularExpression ofRe(R"((\([^()]+\)|[^\s]+)\s+of\s+(\([^()]+\)|[^\s]+))");
    static QRegularExpression timesRe(R"(([^\s]+)\s+times\s+([^\s]+))");
    static QRegularExpression plusRe(R"(([^\s]+)\s+plus\s+([^\s]+))");
    static QRegularExpression byRe(R"(([^\s]+)\s+by\s+([^\s]+))");
    static QRegularExpression intoRe(R"(([^\s]+)\s+into\s+([^\s]+))");
    static QRegularExpression byNoSpaceRe(R"((\d+)by(\d+))",
        QRegularExpression::CaseInsensitiveOption);

    result.replace(byNoSpaceRe, "(\\1/\\2)");

    bool changed;
    do {
        changed = false;
        auto rep = [&](QRegularExpression& re, const QString& repl) {
            QString next = result;
            next.replace(re, repl);
            if (next != result) { result = next; changed = true; }
            };
        rep(ofRe, "(\\1 * \\2)");
        rep(timesRe, "(\\1 * \\2)");
        rep(plusRe, "(\\1 + \\2)");
        rep(byRe, "(\\1 / \\2)");
        rep(intoRe, "(\\1 * \\2)"); // FIX 7: multiplication, not division
    } while (changed);

    static const QRegularExpression spaceParenRe(R"(\s\( \()");
    result.replace(spaceParenRe, "((");
    result.replace(") (", ")*(");

    // FIX 8: all qDebug character-dump loops removed.

    return result;
}

void NaturalLanguage::warmUp()
{
    // Force the fullMap to be built on the first call so preprocess()
    // doesn't pay the construction cost on its first real use.
    (void)expandNumberWords("");
}