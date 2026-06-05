#include "Algebra.h"
#include "Polynomial.h"
#include "Solver.h"
#include "Expr.h"
#include <cmath>
#include <QDebug>

static bool getQuadraticCoefficients(const QString& expr, double& a, double& b, double& c) {
    VarMap vm;
    bool ok;
    vm["x"] = 0.0;
    double f0 = Expr::evalWith(expr, vm, ok);
    if (!ok) return false;
    vm["x"] = 1.0;
    double f1 = Expr::evalWith(expr, vm, ok);
    if (!ok) return false;
    vm["x"] = 2.0;
    double f2 = Expr::evalWith(expr, vm, ok);
    if (!ok) return false;
    c = f0;
    a = (f2 - 2 * f1 + f0) / 2.0;
    b = f1 - a - c;
    return true;
}

QString Algebra::solveEquation(const QString& lhs, const QString& rhs) {
    // Build expression f(x) = lhs - rhs
    QString combined = "(" + lhs + ")-(" + rhs + ")";

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