#include "NaturalLanguage.h"
#include <QRegularExpression>

QString NaturalLanguage::preprocess(const QString& input) {
    QString result = input.toLower();

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


    // "of" -> "*"
    static QRegularExpression ofRe(R"(([^\s]+)\s+of\s+([^\s]+))");
    result.replace(ofRe, "(\\1 * \\2)");

    // "times" -> "*"
    static QRegularExpression timesRe(R"(([^\s]+)\s+times\s+([^\s]+))");
    result.replace(timesRe, "(\\1 * \\2)");

    // "by" -> "/"
    static QRegularExpression byRe(R"(([^\s]+)\s+by\s+([^\s]+))");
    result.replace(byRe, "(\\1 / \\2)");

    // "into" -> swapped division: X into Y -> (Y / X)
    static QRegularExpression intoRe(R"(([^\s]+)\s+into\s+([^\s]+))");
    result.replace(intoRe, "(\\2 / \\1)");

    // "percent" -> /100 (handles standalone and "of" patterns)
 //   static QRegularExpression percentRe(R"((\d+(?:\.\d+)?)\s*percent(?:\s+of\s+)?)");
 //   result.replace(percentRe, "(\\1/100)*");

    return result;
}