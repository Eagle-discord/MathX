#pragma once
#include <QString>
#include "BigNum.h"

struct Polynomial {
    BigDec a = 0;
    BigDec b = 0;
    BigDec c = 0;
};

namespace PolynomialUtils {
    // Parse a string like "x^2 - 5x + 6" into coefficients.
    // Returns true if successful.
    bool parse(const QString& expr, Polynomial& poly);
}