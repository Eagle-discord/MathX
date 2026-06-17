#include "NaturalLanguage.h"
#include <QRegularExpression>

QString NaturalLanguage::preprocess(const QString& input) {
    QString result = input.toLower();

    result.remove(",");
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
        {"half", "(1/2)"},
        {"third", "(1/3)"},
        {"eighth", "(1/8)"},
    };
    for (const auto& [word, repl] : fractions) {
        result.replace(word, repl);
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