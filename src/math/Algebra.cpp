#include "Algebra.h"
#include "Polynomial.h"
#include "PolyEngine.h"
#include "Solver.h"
#include "Expr.h"
#include <cmath>
#include <QDebug>

static bool getQuadraticCoefficients(const QString& expr, double& a, double& b, double& c) {
    // Detect which variable is in the expression instead of assuming "x"
    QSet<QString> vars = Expr::detectVariables(expr);
    if (vars.size() != 1) return false;
    QString var = *vars.begin();

    VarMap vm;
    bool ok;
    vm[var] = 0.0;
    double f0 = Expr::evalWith(expr, vm, ok);
    if (!ok) return false;
    vm[var] = 1.0;
    double f1 = Expr::evalWith(expr, vm, ok);
    if (!ok) return false;
    vm[var] = 2.0;
    double f2 = Expr::evalWith(expr, vm, ok);
    if (!ok) return false;
    c = f0;
    a = (f2 - 2 * f1 + f0) / 2.0;
    b = f1 - a - c;
    return true;
}

// Build a single Poly representing (lhs) - (rhs), if both sides are pure
// single-variable polynomials. Returns false otherwise. Handles the common
// case where sides contain parentheses/superscripts by leaning on Expr's
// preprocessing then PolyEngine's flat parser — but since PolyEngine::parse
// expects a flat form, we first try direct parse and fall back to expansion
// via sampling for the degree<=2 path handled by the caller.
static bool buildEquationPoly(const QString& lhs, const QString& rhs, Poly& out) {
    QString L = Expr::preprocess(lhs);
    QString R = Expr::preprocess(rhs);
    Poly pl, pr;
    if (!PolyEngine::parse(L, pl)) return false;
    if (!PolyEngine::parse(R, pr)) return false;
    // Variables must match (or one side constant).
    if (pl.var != pr.var) {
        // allow if one side has no variable (degree 0)
        if (pl.degree() == 0) pl.var = pr.var;
        else if (pr.degree() == 0) pr.var = pl.var;
        else return false;
    }
    out = pl - pr;
    out.trim();
    return true;
}

QString Algebra::solveEquation(const QString& lhs, const QString& rhs) {
    // Build expression f(x) = lhs - rhs
    QString combined = "(" + lhs + ")-(" + rhs + ")";

    // ── Cubic and higher: route to PolyEngine ──────────────────────────────
    // The sampling method below only recovers up to a quadratic. For anything
    // of degree >= 3 we parse an exact polynomial and solve it properly.
    //
    // NOTE: buildEquationPoly uses PolyEngine's *flat* parser, so an already-
    // expanded form like "x^3 - 6x^2 + 11x - 6 = 0" works, but a factored form
    // like "(x-1)(x-2)(x-3) = 0" does not parse here and will fall through to
    // the sampling path (which handles it only up to degree 2). Expanding
    // bracketed higher-degree input is a possible future enhancement.
    {
        Poly eqPoly;
        if (buildEquationPoly(lhs, rhs, eqPoly) && eqPoly.degree() >= 3) {
            QStringList roots = PolyEngine::solveRoots(eqPoly);
            if (!roots.isEmpty()) return roots.join("\n");
        }
    }

    // Try quadratic via numeric evaluation (works for any quadratic, even with parentheses)
    double a, b, c;
    if (getQuadraticCoefficients(combined, a, b, c)) {
        if (std::abs(a) > 1e-12) {
            // Quadratic
            QStringList roots = Solver::solveQuadratic(a, b, c);
            if (!roots.isEmpty()) return roots.join("\n");
        }
        else if (std::abs(b) > 1e-12) {
            // Linear
            double x = -c / b;
            double rounded = std::round(x);
            if (std::abs(x - rounded) < 1e-9) x = rounded;
            return QString("x = %1").arg(x, 0, 'g', 10);
        }
        else {
            return std::abs(c) < 1e-12 ? "Infinite solutions" : "No solution";
        }
    }

    // Fallback to linear detection (fast method, redundant but safe)
    QSet<QString> vars = Expr::detectVariables(lhs + rhs);
    if (vars.size() == 1) {
        QString varName = *vars.begin();
        QString combined2 = "(" + lhs + ")-(" + rhs + ")";
        VarMap vm0, vm1, vm2;
        vm0[varName] = 0.0; vm1[varName] = 1.0; vm2[varName] = 2.0;
        bool ok0, ok1, ok2;
        double f0 = Expr::evalWith(combined2, vm0, ok0);
        double f1 = Expr::evalWith(combined2, vm1, ok1);
        double f2 = Expr::evalWith(combined2, vm2, ok2);
        if (ok0 && ok1 && ok2) {
            double a_lin = f1 - f0;
            double b_lin = f0;
            if (std::abs(f2 - (2.0 * a_lin + b_lin)) < 1e-9) {
                if (std::abs(a_lin) < 1e-12)
                    return std::abs(b_lin) < 1e-9 ? "Infinite solutions" : "No solution";
                double x = -b_lin / a_lin;
                double rounded = std::round(x);
                if (std::abs(x - rounded) < 1e-9) x = rounded;
                return QString("%1 = %2").arg(varName).arg(x, 0, 'g', 10);
            }
        }
    }

    return "Unsupported equation type";
}

// ---------------------------------------------------------------------------
//  Factoring — delegates to PolyEngine after parsing a single-variable poly.
// ---------------------------------------------------------------------------
QString Algebra::factor(const QString& expr) {
    QString e = Expr::preprocess(expr).trimmed();

    // PolyEngine's parser is flat (no parentheses). If the whole expression is
    // wrapped in a single redundant pair of parens — common after expanding a
    // user function like factor(f(x)) into (x^2 - 5x + 6) — strip it so the
    // flat polynomial underneath can be parsed.
    while (e.startsWith('(') && e.endsWith(')')) {
        // Verify the leading '(' matches the trailing ')' (not "(a)+(b)").
        int depth = 0; bool wraps = true;
        for (int i = 0; i < e.size(); ++i) {
            if (e[i] == '(') ++depth;
            else if (e[i] == ')') { if (--depth == 0 && i != e.size() - 1) { wraps = false; break; } }
        }
        if (!wraps) break;
        e = e.mid(1, e.size() - 2).trimmed();
    }

    Poly p;
    if (!PolyEngine::parse(e, p)) return {};
    return PolyEngine::factor(p);
}

// ---------------------------------------------------------------------------
//  Linear systems — delegates to PolyEngine.
// ---------------------------------------------------------------------------
QString Algebra::solveSystem(const QStringList& equations) {
    QStringList result = PolyEngine::solveLinearSystem(equations);
    if (result.isEmpty()) return {};
    return result.join("\n");
}