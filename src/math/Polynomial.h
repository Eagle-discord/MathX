#pragma once
#include <QString>

struct Polynomial {
    double a = 0.0; // x^2 coefficient
    double b = 0.0; // x coefficient
    double c = 0.0; // constant
};

namespace PolynomialUtils {
    // Parse a string like "x^2 - 5x + 6" into coefficients.
    // Returns true if successful.
    bool parse(const QString& expr, Polynomial& poly);
}