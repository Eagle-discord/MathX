#pragma once
#include <QString>
#include <QStringList>

namespace Algebra {
    // Solve an equation of the form lhs = rhs in a single variable.
    // Handles linear, quadratic, and (via PolyEngine) cubic+ polynomials.
    // Returns a human-readable solution string (e.g., "x = 2", "x₁ = 2, x₂ = 3").
    // Returns empty string if unsolvable or variable detection fails.
    QString solveEquation(const QString& lhs, const QString& rhs);

    // Factor a single-variable polynomial, e.g. "x^2 - 5x + 6" -> "(x - 2)(x - 3)".
    // Returns empty string if the input isn't a factorable polynomial or is
    // already irreducible.
    QString factor(const QString& expr);

    // Solve a system of linear equations given as a list of "lhs = rhs" strings.
    // Returns one solution line per variable, or a single line describing
    // no/infinite solutions. Empty string if it can't be parsed as a linear
    // system.
    QString solveSystem(const QStringList& equations);
}