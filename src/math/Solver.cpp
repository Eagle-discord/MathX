#include "Solver.h"
#include "Expr.h"      // for evalWith
#include <cmath>
#include <stdexcept>
#include "MathEngine.h"

static double solveFor(const QString& lhs, const QString& rhs,
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

    auto f = [&](double x) { return evalSide(lhs, x) - evalSide(rhs, x); };

    auto df = [&](double x) {
        // Use a relative h to prevent precision loss for large x
        double h = std::max(1.0, std::abs(x)) * 1e-7;
        return (f(x + h) - f(x - h)) / (2.0 * h);
        };

    for (double start : {0.0, 1.0, -1.0, 10.0, -10.0, 0.5, 100.0}) {
        try {
            double x = start;
            for (int i = 0; i < 500; ++i) {
                double fx = f(x);
                double dfx = df(x);

                if (std::abs(dfx) < 1e-15) break; // Avoid division by zero

                double xn = x - fx / dfx;

                if (std::abs(xn - x) < 1e-10 && std::abs(f(xn)) < 1e-8) {
                    ok = true;
                    return xn;
                }
                x = xn;
                if (!std::isfinite(x)) break;
            }
        }
        catch (...) {}
    }
    return 0.0;
}

double Solver::solveLinear(const QString& lhs, const QString& rhs,
    const QString& varName, bool& ok) {
    return solveFor(lhs, rhs, varName, ok);
}
QStringList Solver::solveQuadratic(double a, double b, double c) {
    auto fmt = [](double v) -> QString {
        if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
        if (std::isnan(v)) return "Undefined";
        return QString::number(v, 'g', 10);
        };

    double disc = b * b - 4 * a * c;

    if (disc > 0) {
        // Stable calculation of roots
        double q = -0.5 * (b + (b > 0 ? 1.0 : -1.0) * std::sqrt(disc));
        double x1 = q / a;
        double x2 = c / q;
        return { QString("x₁ = %1").arg(fmt(x1)),
                 QString("x₂ = %1").arg(fmt(x2)) };
    }
    else if (std::abs(disc) < 1e-12) {
        return { QString("x = %1 (double root)").arg(fmt(-b / (2 * a))) };
    }
    else {
        double re = -b / (2 * a);
        double im = std::sqrt(-disc) / (2 * a);
        return { QString("x₁ = %1 + %2i").arg(fmt(re)).arg(fmt(im)),
                 QString("x₂ = %1 - %2i").arg(fmt(re)).arg(fmt(im)) };
    }
}
