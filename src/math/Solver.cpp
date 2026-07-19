#include "Solver.h"
#include "Expr.h"
#include "MathEngine.h"
#include "../math/BigNum.h"
#include <cmath>
#include <stdexcept>

// ── Linear solver using BigDec (fast, exact for linear) ─────────────────────
double Solver::solveLinear(const QString& lhs, const QString& rhs,
    const QString& varName, bool& ok) {
    ok = false;

    auto evalSide = [&](const QString& side, double val) -> double {
        VarMap vm;
        vm[varName] = val;
        bool b;
        double v = Expr::evalWith(side, vm, b);
        if (!b) throw std::runtime_error("eval failed");
        return v;
        };

    // Evaluate f(x) = lhs - rhs at x=0 and x=1
    double f0_double, f1_double;
    try {
        f0_double = evalSide(lhs, 0.0) - evalSide(rhs, 0.0);
        f1_double = evalSide(lhs, 1.0) - evalSide(rhs, 1.0);
    }
    catch (...) {
        return 0.0;
    }

    BigDec f0(f0_double);
    BigDec f1(f1_double);

    // Linear coefficients: a = f1 - f0, b = f0
    BigDec a = f1 - f0;
    BigDec b = f0;

    // Check for near-zero a
    if (BigNum::abs(a) < BigDec("1e-12")) {
        // If b is also near zero, infinite solutions; otherwise no solution.
        if (BigNum::abs(b) < BigDec("1e-12")) {
            ok = true;  // Infinite solutions (we'll treat as any value)
            return 0.0; // Caller handles this
        }
        return 0.0; // No solution
    }

    BigDec x = -b / a;
    ok = true;
    // Convert result back to double (safe as it's within double range)
    double result = static_cast<double>(x);
    // Round to nearest integer if close
    double rounded = std::round(result);
    if (std::abs(result - rounded) < 1e-9)
        result = rounded;
    return result;
}

// ── Quadratic solver using BigDec ────────────────────────────────────────────
QStringList Solver::solveQuadratic(double a, double b, double c) {
    BigDec A(a), B(b), C(c);
    BigDec disc = B * B - 4 * A * C;

    auto fmt = [](const BigDec& v) -> QString {
        return BigNum::fmt(v);
        };

    if (disc > 0) {
        BigDec q = -0.5 * (B + (B > 0 ? BigDec(1) : BigDec(-1)) * BigNum::sqrt(disc));
        BigDec x1 = q / A;
        BigDec x2 = C / q;
        return { QString("x₁ = %1").arg(BigNum::fmt(x1)),
                 QString("x₂ = %1").arg(BigNum::fmt(x2)) };
    }
    else if (disc == 0) {
        BigDec x = -B / (2 * A);
        return { QString("x = %1 (double root)").arg(BigNum::fmt(x)) };
    }
    else {
        BigDec re = -B / (2 * A);
        BigDec im = BigNum::sqrt(-disc) / (2 * A);
        return { QString("x₁ = %1 + %2i").arg(BigNum::fmt(re)).arg(BigNum::fmt(im)),
                         QString("x₂ = %1 - %2i").arg(BigNum::fmt(re)).arg(BigNum::fmt(im)) };
    }
}