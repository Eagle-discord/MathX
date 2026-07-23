#pragma once
//  TreeSolver - solve equations using the AST, covering:
//    1. Polynomial equations with terms on BOTH sides:  2x + 3 = x - 5
//    2. Bracketed / factored forms:                     (x-1)(x-2)(x-3) = 0
//    3. Rational equations (variable in denominator):    x/(x-2) = 3
//    4. Radical and absolute-value equations:            sqrt(x+1) = 4, |x-3| = 5
//
//  Strategy: parse both sides into trees, then classify.
//    * If lhs - rhs extracts to a polynomial  -> solve exactly (delegates to
//      PolyEngine/Solver for the actual roots), which subsumes cases 1 & 2.
//    * If it's a single rational equation      -> cross-multiply on the tree,
//      solve the resulting polynomial, discard roots that hit an excluded value
//      (denominator = 0).
//    * If a side is a lone sqrt(...) or |...|  -> invert structurally and check
//      each candidate back in the original equation (extraneous-root guard).
//
//  Absolute value in the input may be written with pipes: |expr|. The caller
//  should pass the raw string so TreeSolver can pre-process pipes into abs(...).

#include <QString>
#include <QStringList>

namespace TreeSolver {
    // Attempt to solve "lhsRaw = rhsRaw". Returns human-readable solution lines
    // (e.g. {"x = 2"} or {"x₁ = 3", "x₂ = -3"}), or an empty list if this solver
    // doesn't recognise/handle the equation (caller falls back to older paths).
    //
    // `note` (optional) receives side information such as excluded values or an
    // "extraneous root discarded" remark.
    QStringList solve(const QString& lhsRaw, const QString& rhsRaw, QString* note = nullptr);

    // Convenience: split a full "lhs = rhs" string and solve. Returns empty if
    // there isn't exactly one '='.
    QStringList solveEquation(const QString& equationRaw, QString* note = nullptr);

    // Expose the pipe->abs preprocessing (also handy for the caller/tests).
    QString normalizeAbsBars(const QString& s);
}
