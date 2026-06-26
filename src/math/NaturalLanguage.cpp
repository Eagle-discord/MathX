#include "NaturalLanguage.h"
#include <QRegularExpression>

#include <QMap>
#include <QStringList>
#include <boost/multiprecision/cpp_int.hpp>
using boost::multiprecision::cpp_int;

#include <QString>
#include <QStringList>
#include <QVector>
static cpp_int pow10(int exp)
{
    cpp_int r = 1;
    while (exp--)
        r *= 10;
    return r;
}
QString makeConwayGuyName(int zeros)
{
    if (zeros < 0)
        throw std::invalid_argument("zeros must be >= 0");

    // Small names
    if (zeros < 3)  return "";
    if (zeros < 6)  return "thousand";
    if (zeros < 9)  return "million";
    if (zeros < 12) return "billion";
    if (zeros < 15) return "trillion";
    if (zeros < 18) return "quadrillion";
    if (zeros < 21) return "quintillion";
    if (zeros < 24) return "sextillion";
    if (zeros < 27) return "septillion";
    if (zeros < 30) return "octillion";
    if (zeros < 33) return "nonillion";

    static const QString names[4][10] =
    {
        { "", "un", "do", "tre", "quattuor", "quin", "se", "septe", "octo", "nove" },

        { "", "deci", "viginti", "triginta", "quadraginta",
          "quinquaginta", "sexaginta", "septuaginta",
          "octoginta", "nonaginta" },

        { "", "centi", "ducenti", "trecenti", "quadringenti",
          "quingenti", "sescenti", "septingenti",
          "octingenti", "nongenti" },

        { "", "milli", "billi", "trilli", "quadrilli",
          "quintilli", "sextilli", "septilli",
          "octilli", "nonilli" }
    };

    static const QStringList rules[4][10] =
    {
        {
            {}, {}, {}, {"s"}, {}, {}, {"s","x"}, {"m","n"}, {}, {"m","n"}
        },
        {
            {}, {"n"}, {"m","s"}, {"n","s"}, {"n","s"},
            {"n","s"}, {"n"}, {"n"}, {"m","x"}, {}
        },
        {
            {}, {"n","x"}, {"n"}, {"n","s"}, {"n","s"},
            {"n","s"}, {"n"}, {"n"}, {"m","x"}, {}
        },
        {
            {}, {}, {}, {}, {}, {}, {}, {}, {}, {}
        }
    };

    int group = (zeros - 3) / 3;

    QVector<int> digits;

    while (group > 0)
    {
        digits.push_back(group % 10);
        group /= 10;
    }

    QVector<QString> out(digits.size());

    for (int i = digits.size() - 1; i >= 0; --i)
    {
        if (i >= 4)
            throw std::runtime_error("Number too large");

        out[i] = names[i][digits[i]];

        if (i == 0)
        {
            QStringList lastRules = rules[1][digits.back()];
            QStringList myRules = rules[0][digits[0]];

            for (const QString& r : myRules)
            {
                if (lastRules.contains(r))
                    out[0] += r;
            }
        }
    }

    QString result;

    for (const QString& s : out)
        result += s;

    if (result.endsWith('a'))
    {
        result.chop(1);
        result += 'i';
    }

    result[0] = result[0].toUpper();

    return result + "llion";
}
QMap<QString, cpp_int> generateIllionMap()
{
    QMap<QString, cpp_int> map;

    for (int zeros = 6; zeros <= 3003; zeros += 3)
    {
        map.insert(
            makeConwayGuyName(zeros).toLower(),
            pow10(zeros));
    }

    return map;
}


static QString expandNumberWords(const QString& input) {
    // Word → value map (units, teens, tens, scales) as cpp_int
    static const QMap<QString, cpp_int> wordMap = {
        {"zero", 0}, {"one", 1}, {"two", 2}, {"three", 3}, {"four", 4},
        {"five", 5}, {"six", 6}, {"seven", 7}, {"eight", 8}, {"nine", 9},
        {"ten", 10}, {"eleven", 11}, {"twelve", 12}, {"thirteen", 13},
        {"fourteen", 14}, {"fifteen", 15}, {"sixteen", 16}, {"seventeen", 17},
        {"eighteen", 18}, {"nineteen", 19},
        {"twenty", 20}, {"thirty", 30}, {"forty", 40}, {"fifty", 50},
        {"sixty", 60}, {"seventy", 70}, {"eighty", 80}, {"ninety", 90},
        {"hundred",      pow10(2)},
{"thousand",     pow10(3)},
{"million",      pow10(6)},
{"billion",      pow10(9)},
{"trillion",     pow10(12)},
{"quadrillion",  pow10(15)},
{"quintillion",  pow10(18)},
{"sextillion",   pow10(21)},
{"septillion",   pow10(24)},
{"octillion",    pow10(27)},
{"nonillion",    pow10(30)},
{"decillion",    pow10(33)},
{"undecillion",  pow10(36)},
{"duodecillion", pow10(39)},

{"vigintillion", pow10(63)},

{"trigintillion", pow10(93)},

{"quadragintillion", pow10(123)},

{"quinquagintillion", pow10(153)},

{"sexagintillion", pow10(183)},

{"septuagintillion", pow10(213)},

{"octogintillion", pow10(243)},

{"nonagintillion", pow10(273)},

{"centillion", pow10(303)},

{"ducentillion",      pow10(603)},
{"trucentillion",     pow10(903)},
{"quadringentillion", pow10(1203)},
{"quingentillion",    pow10(1503)},
{"sescentillion",     pow10(1803)},
{"septingentillion",  pow10(2103)},
{"octingentillion",   pow10(2403)},
{"nongentillion",     pow10(2703)},
{"millinillion",      pow10(3003)}
    
    };
    static const QSet<QString> ignoreWords = { "and" };

    QStringList tokens = input.split(QRegularExpression(R"(\s+)"), Qt::SkipEmptyParts);
    QStringList output;
    int i = 0;
    while (i < tokens.size()) {
        QString token = tokens[i].toLower();
        if (wordMap.contains(token)) {
            cpp_int total = 0;
            cpp_int current = 0;
            int j = i;
            while (j < tokens.size()) {
                QString w = tokens[j].toLower();
                if (ignoreWords.contains(w)) { j++; continue; }
                if (!wordMap.contains(w)) break;
                cpp_int n = wordMap[w];
                if (n == 100) {
                    if (current == 0)
                        current = 1;

                    current *= 100;
                }
                else if (n >= 1000) {
                    if (current == 0)
                        current = 1;

                    total += current * n;
                    current = 0;
                }
                else {
                    current += n;
                }
          
                j++;
            }
            // Combine any remaining current
            if (current > 0) total += current;
            if (total != 0 || j > i) {
                // It's a number sequence
                output.append(QString::fromStdString(total.str()));
                i = j;
                continue;
            }
        }
        // Not a number word, just append
        output.append(tokens[i]);
        i++;
    }
    return output.join(" ");
}

QString normalizeNumericFormatting(QString input)
{

    QRegularExpression thousandsSep(R"((\d),(?=\d{3}(\D|$)))");
    while(input.contains(thousandsSep))
    input.replace(thousandsSep, "\\1");
    return input;
}
QString NaturalLanguage::preprocess(const QString& input) {
    qDebug() << makeConwayGuyName(303);
    qDebug() << makeConwayGuyName(603);
    qDebug() << makeConwayGuyName(903);
    QString result = input.toLower();
    auto map = generateIllionMap();

    qDebug() << map.contains("centillion");

    cpp_int c = map["centillion"];

    qDebug() << QString::fromStdString(c.str());
    QString expanded = expandNumberWords(input);
    qDebug() << expanded;
    result = expandNumberWords(result);
    result = normalizeNumericFormatting(result);
    // 1. Convert "5 percent" -> "5%" (must be done before "of" etc.)
    static QRegularExpression percentWordRe(R"((\d+(?:\.\d+)?)\s+percent\b)");
    result.replace(percentWordRe, "\\1%");

    // 2. Fraction words (longest first)
    static const std::vector<std::pair<QString, QString>> fractions = {
        {"three quarters", "(3/4)"},
        {"two thirds", "(2/3)"},
        {"one quarter", "(1/4)"},
        {"one third", "(1/3)"},
        {"a quarter", "(1/4)"},
        {"a half", "(1/2)"},
        {"quarter", "(1/4)"},
        {"one fourth", "(1/4)"},
        {"half", "(1/2)"},
        {"third", "(1/3)"},
        {"eighth", "(1/8)"},
    };
    for (const auto& [word, repl] : fractions) {
        result.replace(word, repl);
    }
    // 2. Word numbers (things like "a dozen"
    static const std::vector<std::pair<QString, QString>> num_units = {
        {"dozen", "(12)"},
    };
    for (const auto& [word, repl] : num_units) {
        result.replace(word, repl);
    }

    static const QStringList prefixes = {
    "what is",
    "calculate",
    "compute",
    "evaluate",
    "find"
    };

    for (const QString& p : prefixes) {
        if (result.startsWith(p, Qt::CaseInsensitive)) {
            result = result.mid(p.length()).trimmed();
            break;
        }
    }

    // Convert "mach 1" → "1 mach" (unit before number)
    static QRegularExpression machRe(R"(\bmach\s+(\d+(?:\.\d+)?)\b)", QRegularExpression::CaseInsensitiveOption);
    result.replace(machRe, "\\1 mach");

    // "of" -> "*"
    static QRegularExpression ofRe(R"((\([^()]+\)|[^\s]+)\s+of\s+(\([^()]+\)|[^\s]+))");
    result.replace(ofRe, "(\\1 * \\2)");


    // Handle "123by3" -> "123/3"  (no spaces)
    static QRegularExpression byNoSpaceRe(R"((\d+)by(\d+))", QRegularExpression::CaseInsensitiveOption);
    result.replace(byNoSpaceRe, "(\\1/\\2)");

    // Similarly handle "123into243" -> "(243/123)"? We'll skip for simplicity.
    // "times" -> "*"
    static QRegularExpression timesRe(R"(([^\s]+)\s+times\s+([^\s]+))");
    result.replace(timesRe, "(\\1 * \\2)");

    // "by" -> "/"
    static QRegularExpression byRe(R"(([^\s]+)\s+by\s+([^\s]+))");
    result.replace(byRe, "(\\1 / \\2)");

    // "into" -> swapped division: X into Y -> (Y / X)
    static QRegularExpression intoRe(R"(([^\s]+)\s+into\s+([^\s]+))");
    result.replace(intoRe, "(\\1 / \\2)");

    // "percent" -> /100 (handles standalone and "of" patterns)
 //   static QRegularExpression percentRe(R"((\d+(?:\.\d+)?)\s*percent(?:\s+of\s+)?)");
 //   result.replace(percentRe, "(\\1/100)*");
    // Normalize spaces around parentheses
   // result.replace(QRegularExpression(R"(\s*\()"), "(");
  //  result.replace(QRegularExpression(R"(\)\s*)"), ")");
    result.replace(QRegularExpression(R"(\s\( \()"), "((");
    result.replace(") (", ")*(");
    QString old;
    do {
        old = result;
        // "of" -> "*"
        result.replace(ofRe, "(\\1 * \\2)");
        // "times" -> "*"
        result.replace(timesRe, "(\\1 * \\2)");
        // "by" -> "/"
        result.replace(byRe, "(\\1 / \\2)");
        // "into" -> swapped division
        result.replace(intoRe, "(\\2 / \\1)");
    } while (result != old);
    return result;
}