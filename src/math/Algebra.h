#pragma once
#include <QString>
#include <QStringList>

namespace Algebra {
    // Solve an equation of the form lhs = rhs in a single variable.
    // Returns a human-readable solution string (e.g., "x = 2", "x₁ = 2, x₂ = 3").
    // Returns empty string if unsolvable or variable detection fails.
    QString solveEquation(const QString& lhs, const QString& rhs);
}