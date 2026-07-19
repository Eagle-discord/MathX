#pragma once
#include <QString>

namespace NaturalLanguage {
    // Convert natural language phrases to math expressions.
    // Examples:
    //   "half of 100" -> "(1/2)*100"
    //   "2 into 10"   -> "(10/2)"
    //   "quarter of 80" -> "(1/4)*80"
    //   "50% of 200" -> "(50/100)*200"
    QString preprocess(const QString& input);
    // We will call this once early (e.g., after the main window shows) to pre-build the maps.
    void warmUp();
    constexpr int MAX_ZEROS = 3003;
}