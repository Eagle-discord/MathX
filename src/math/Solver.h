#pragma once
#include <QString>
#include <QStringList>

namespace Solver {
    // Solve for a single variable using Newton's method.
    // Equation is lhs == rhs (strings).
    double solveLinear(const QString& lhs, const QString& rhs,
        const QString& varName, bool& ok);

    // Solve ax² + bx + c = 0, returning human-readable roots.
    QStringList solveQuadratic(double a, double b, double c);
}