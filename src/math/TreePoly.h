#pragma once
//  TreePoly - extract an exact univariate polynomial from an AST.
//
//  This replaces numeric sampling for the polynomial cases. Instead of
//  evaluating f at x=0,1,2,... and back-solving coefficients (which forces you
//  to pre-guess the degree and breaks on brackets), it walks the tree and
//  combines child polynomials symbolically:
//
//      poly(a + b) = poly(a) + poly(b)
//      poly(a * b) = poly(a) * poly(b)     <-- expands (x-1)(x-2)(x-3) exactly
//      poly(a ^ n) = poly(a) raised to integer n
//
//  A "polynomial" here is a dense BigDec coefficient vector, low-order first,
//  tagged with the single variable it is in. Extraction fails (ok=false) if the
//  expression isn't a polynomial in one variable - e.g. it divides by the
//  variable, raises to a non-integer/negative power, or contains a transcend-
//  ental function of the variable. Constant subexpressions (including things
//  like sqrt(2) or pi) are fine and fold into coefficients.

#include <QString>
#include <QVector>
#include "ExprTree.h"
#include "BigNum.h"

struct UniPoly {
    QVector<BigDec> coeffs;   // low-order first; coeffs[i] * var^i
    QString var;              // the single variable (empty if constant)
    bool ok = false;          // set true on successful extraction

    int degree() const {
        for (int i = coeffs.size() - 1; i >= 0; --i)
            if (coeffs[i] != 0) return i;
        return 0;
    }
};

namespace TreePoly {
    // Extract a univariate polynomial from the tree. `expectedVar` may be empty
    // (auto-detect the single variable) or a specific name to require.
    UniPoly extract(const NodePtr& n, const QString& expectedVar = QString());

    // Count distinct variables in the tree (used to reject multivariable input
    // before attempting single-variable extraction).
    QStringList variables(const NodePtr& n);
}
