#pragma once
#include <QString>

namespace Simplifier {
    // Simplify an algebraic expression (single variable, + - * ^).
    // Returns the simplified string, or empty if no change or error.
    QString simplify(const QString& expr);
}