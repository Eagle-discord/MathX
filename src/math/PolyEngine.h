#pragma once
//  PolyEngine — a dense, arbitrary-degree univariate polynomial over BigDec,
//  plus the algebra operations built on top of it:
//    * parsing "3x^2 - 5x + 6" (any single variable, any degree)
//    * add / sub / mul / scalar / derivative
//    * rational-root factoring over the integers
//    * root finding (linear, quadratic exact; cubic+ numeric via deflation)
//    * small linear-system solver (2–3 unknowns) via Gaussian elimination
//
//  This sits ALONGSIDE the existing numeric-sampling path in Algebra.cpp.
//  It uses BigDec (from BigNum.h) so results keep the same precision as the
//  rest of the engine.

#include <QString>
#include <QStringList>
#include <QVector>
#include "BigNum.h"

// ---------------------------------------------------------------------------
//  Poly: coefficients stored low-order first.
//  coeffs[i] is the coefficient of x^i, so {6, -5, 3} == 3x^2 - 5x + 6.
// ---------------------------------------------------------------------------
class Poly {
public:
    QVector<BigDec> coeffs;   // low-order first
    QString var = "x";        // the variable this polynomial is written in

    Poly() = default;
    explicit Poly(QVector<BigDec> c, QString v = "x")
        : coeffs(std::move(c)), var(std::move(v)) {
        trim();
    }

    // Degree of the polynomial (degree of the zero polynomial is reported as 0).
    int degree() const {
        for (int i = coeffs.size() - 1; i >= 0; --i)
            if (coeffs[i] != 0) return i;
        return 0;
    }
    bool isZero() const {
        for (const BigDec& c : coeffs) if (c != 0) return false;
        return true;
    }
    BigDec at(int i) const {
        return (i >= 0 && i < coeffs.size()) ? coeffs[i] : BigDec(0);
    }
    // Evaluate p(x) with Horner's method.
    BigDec eval(const BigDec& x) const {
        BigDec acc = 0;
        for (int i = coeffs.size() - 1; i >= 0; --i)
            acc = acc * x + coeffs[i];
        return acc;
    }

    // Arithmetic
    Poly operator+(const Poly& o) const;
    Poly operator-(const Poly& o) const;
    Poly operator*(const Poly& o) const;
    Poly scaled(const BigDec& k) const;
    Poly derivative() const;

    // Drop trailing (high-order) zero coefficients; keep at least a constant.
    void trim();

    // Pretty-print, e.g. "3x^2 - 5x + 6".
    QString toString() const;

private:
    static QString fmtCoeff(const BigDec& v);
};

namespace PolyEngine {

    // -- Parsing ---------------------------------------------------------------
    // Parse a flat polynomial in ONE variable: "5x^2 + 1", "x^3 - 2x + 4",
    // "-x", "2x²" (superscripts already normalised upstream is fine, but we
    // also handle x²/x³ here defensively). Returns false if the string is not
    // a pure single-variable polynomial (e.g. it has functions, two variables,
    // or division by the variable).
    bool parse(const QString& expr, Poly& out);

    // -- Factoring -------------------------------------------------------------
    // Factor a polynomial with (near-)integer coefficients into linear/quadratic
    // factors over the rationals using the rational-root theorem + deflation.
    // Returns a display string like "(x - 2)(x - 3)" or "2(x + 1)(x - 3)".
    // Returns empty string if no non-trivial factorisation is found.
    QString factor(const Poly& p);

    // -- Root finding ----------------------------------------------------------
    // Solve p(x) = 0. Handles degree 1 & 2 exactly (real + complex), and
    // degree >= 3 by extracting rational roots then numerically solving the
    // remainder via Durand–Kerner. Returns human-readable root lines.
    QStringList solveRoots(const Poly& p);

    // -- Linear systems --------------------------------------------------------
    // Solve a system of linear equations (each given as "lhs = rhs") in up to
    // a handful of unknowns. Returns lines like "x = 2", "y = -1", or a single
    // line describing no / infinite solutions. Empty result => couldn't parse
    // as a linear system.
    QStringList solveLinearSystem(const QStringList& equations);

    // Convenience: does this string look like a solvable single-variable
    // polynomial equation we should route here? (has '=', one variable, no
    // unsupported functions). Used by Algebra.cpp to decide routing.
    bool looksLikePolynomialEquation(const QString& lhs, const QString& rhs);

}  // namespace PolyEngine