#include "TreeSolver.h"
#include "TreeParser.h"
#include "TreePoly.h"
#include "ExprTree.h"
#include "PolyEngine.h"
#include "BigNum.h"
#include <QRegularExpression>
#include <cmath>
#include <functional>

namespace {

// ---------------------------------------------------------------------------
//  Small utilities
// ---------------------------------------------------------------------------

// Convert a UniPoly (from TreePoly) into a PolyEngine Poly so we can reuse the
// existing exact root solver (linear/quadratic/cubic+).
Poly toEnginePoly(const UniPoly& up) {
    return Poly(up.coeffs, up.var.isEmpty() ? QString("x") : up.var);
}

// Does the tree contain the variable anywhere in a denominator (Div right
// child), i.e. is it a rational expression in the variable?
bool hasVarInDenominator(const NodePtr& n) {
    if (!n) return false;
    if (n->kind == NodeKind::Div) {
        QStringList dv = TreePoly::variables(n->right);
        if (!dv.isEmpty()) return true;
    }
    return hasVarInDenominator(n->left) || hasVarInDenominator(n->right);
}

// Split a rational tree into numerator and denominator trees such that
// node == num/den (den defaults to 1). This is a light normalisation that
// handles the common shapes: a single top-level division, or a sum where we
// place everything over a common denominator by multiplying out. To keep it
// robust and simple, we only special-case:
//    A/B                      -> (A, B)
//    anything else            -> (node, 1)   [caller multiplies through]
// The cross-multiplication step below combines two such fractions.
struct Frac { NodePtr num; NodePtr den; };

Frac asFraction(const NodePtr& n) {
    if (n && n->kind == NodeKind::Div)
        return { n->left, n->right };
    return { n, AST::num(1) };
}

QString fmt(const BigDec& v) { return BigNum::fmt(v); }

// Evaluate a tree at a given numeric value of `var`. ok=false on domain error.
BigDec evalAt(const NodePtr& n, const QString& var, const BigDec& x, bool& ok) {
    VarBindings vb;
    vb.set(var, x);
    ok = true;
    return AST::eval(n, vb, ok);
}

// Parse a numeric root string like "x = 3" or "x₁ = -2.5" back to a BigDec,
// returning false for non-numeric (complex, "double root", etc.) lines so the
// extraneous-root checker can skip them.
bool numericRootOf(const QString& line, BigDec& out) {
    int eq = line.indexOf('=');
    if (eq < 0) return false;
    QString rhs = line.mid(eq + 1).trimmed();
    // strip trailing annotations
    rhs.replace("(double root)", "");
    rhs = rhs.trimmed();
    if (rhs.contains('i')) return false;                 // complex
    bool ok;
    double d = rhs.toDouble(&ok);
    if (!ok) return false;
    out = BigDec(d);
    return true;
}

// ---------------------------------------------------------------------------
//  Case A: polynomial equation (both-sides, brackets) - exact via extraction.
// ---------------------------------------------------------------------------
QStringList solvePolynomial(const NodePtr& lhs, const NodePtr& rhs, bool& handled) {
    handled = false;

    // diff = lhs - rhs
    NodePtr diff = AST::bin(NodeKind::Sub, AST::clone(lhs), AST::clone(rhs));

    // Reject if the variable appears in a denominator (that's the rational
    // case, handled elsewhere).
    if (hasVarInDenominator(diff)) return {};

    UniPoly up = TreePoly::extract(diff);
    if (!up.ok) return {};

    handled = true;
    Poly p = toEnginePoly(up);
    QStringList roots = PolyEngine::solveRoots(p);
    return roots;
}

// ---------------------------------------------------------------------------
//  Case B: rational equation - cross-multiply, solve, drop excluded values.
// ---------------------------------------------------------------------------
QStringList solveRational(const NodePtr& lhs, const NodePtr& rhs,
                          bool& handled, QString* note) {
    handled = false;

    // Only engage if the variable is actually in a denominator somewhere.
    if (!hasVarInDenominator(lhs) && !hasVarInDenominator(rhs)) return {};

    // Represent each side as a fraction, then cross-multiply:
    //   Ln/Ld = Rn/Rd   =>   Ln*Rd - Rn*Ld = 0
    Frac L = asFraction(lhs);
    Frac R = asFraction(rhs);

    NodePtr crossed = AST::bin(NodeKind::Sub,
        AST::bin(NodeKind::Mul, AST::clone(L.num), AST::clone(R.den)),
        AST::bin(NodeKind::Mul, AST::clone(R.num), AST::clone(L.den)));

    UniPoly up = TreePoly::extract(crossed);
    if (!up.ok) return {};                 // denominators weren't polynomial → give up
    handled = true;

    Poly p = toEnginePoly(up);
    QStringList rawRoots = PolyEngine::solveRoots(p);

    QString var = up.var.isEmpty() ? QString("x") : up.var;

    // Determine excluded values: zeros of either denominator.
    QVector<BigDec> excluded;
    auto collectDenZeros = [&](const NodePtr& den) {
        UniPoly d = TreePoly::extract(den);
        if (!d.ok) return;
        Poly dp = toEnginePoly(d);
        for (const QString& rl : PolyEngine::solveRoots(dp)) {
            BigDec rv;
            if (numericRootOf(rl, rv)) excluded.append(rv);
        }
    };
    collectDenZeros(L.den);
    collectDenZeros(R.den);

    // Filter roots that coincide with an excluded value.
    QStringList kept;
    bool dropped = false;
    for (const QString& rl : rawRoots) {
        BigDec rv;
        if (numericRootOf(rl, rv)) {
            bool bad = false;
            for (const BigDec& ex : excluded)
                if (BigNum::abs(rv - ex) < BigDec("1e-9")) { bad = true; break; }
            if (bad) { dropped = true; continue; }
        }
        kept << rl;
    }

    if (dropped && note)
        *note = "Some candidate roots were excluded (they make a denominator zero).";

    if (kept.isEmpty())
        return { QStringLiteral("No solution") };
    return kept;
}

// ---------------------------------------------------------------------------
//  Case C: radical / absolute-value on one side.
// ---------------------------------------------------------------------------
//  Recognise the shapes:
//     sqrt(g(x)) = h        ->  g(x) = h^2        (requires h >= 0)
//     |g(x)|     = h        ->  g(x) = ±h         (requires h >= 0)
//  where h is a constant. g(x) is then solved as a polynomial and every
//  candidate is checked back in the ORIGINAL equation to kill extraneous roots.
// ---------------------------------------------------------------------------
QStringList solveRadicalOrAbs(const NodePtr& lhs, const NodePtr& rhs,
                              bool& handled, QString* note) {
    handled = false;

    // Identify which side is the sqrt/abs wrapper and which is the constant.
    auto isWrapper = [](const NodePtr& n) {
        return n && n->kind == NodeKind::Func &&
               (n->name == "sqrt" || n->name == "abs") && n->args.size() == 1;
    };

    NodePtr wrap, other;
    if (isWrapper(lhs))      { wrap = lhs; other = rhs; }
    else if (isWrapper(rhs)) { wrap = rhs; other = lhs; }
    else return {};

    // The non-wrapper side must be a constant.
    BigDec h;
    {
        VarBindings empty;
        bool ok = true;
        h = AST::eval(other, empty, ok);
        if (!ok) return {};   // not a constant → beyond this simple handler
    }

    handled = true;
    QString fname = wrap->name;
    NodePtr inner = wrap->args[0];

    QString var;
    {
        QStringList vs = TreePoly::variables(inner);
        if (vs.size() != 1) { handled = false; return {}; }
        var = vs.first();
    }

    auto checkBack = [&](const BigDec& cand) -> bool {
        bool ok;
        BigDec lv = evalAt(lhs, var, cand, ok); if (!ok) return false;
        BigDec rv = evalAt(rhs, var, cand, ok); if (!ok) return false;
        return BigNum::abs(lv - rv) < BigDec("1e-7");
    };

    QStringList out;
    bool dropped = false;

    if (fname == "sqrt") {
        // sqrt(inner) = h requires h >= 0; then inner = h^2.
        if (h < 0) return { QStringLiteral("No solution") };
        BigDec hSquared = h * h;                       // materialize before overload resolution
        NodePtr eqTree = AST::bin(NodeKind::Sub, AST::clone(inner), AST::num(hSquared));
        UniPoly up = TreePoly::extract(eqTree);
        if (!up.ok) { handled = false; return {}; }
        Poly p = toEnginePoly(up);
        for (const QString& rl : PolyEngine::solveRoots(p)) {
            BigDec rv;
            if (numericRootOf(rl, rv)) {
                if (checkBack(rv)) out << QString("%1 = %2").arg(var, fmt(rv));
                else dropped = true;
            } else {
                out << rl;   // pass through non-numeric (shouldn't happen for linear inner)
            }
        }
    } else { // abs
        if (h < 0) return { QStringLiteral("No solution") };
        // inner = h  OR  inner = -h
        for (int s = 0; s < 2; ++s) {
            BigDec target = (s == 0) ? h : -h;
            NodePtr eqTree = AST::bin(NodeKind::Sub, AST::clone(inner), AST::num(target));
            UniPoly up = TreePoly::extract(eqTree);
            if (!up.ok) { handled = false; return {}; }
            Poly p = toEnginePoly(up);
            for (const QString& rl : PolyEngine::solveRoots(p)) {
                BigDec rv;
                if (numericRootOf(rl, rv)) {
                    if (checkBack(rv)) {
                        QString line = QString("%1 = %2").arg(var, fmt(rv));
                        if (!out.contains(line)) out << line;
                    } else dropped = true;
                }
            }
        }
    }

    if (dropped && note)
        *note = "Extraneous roots were discarded (they don't satisfy the original equation).";

    if (out.isEmpty())
        return { QStringLiteral("No solution") };
    return out;
}

} // namespace

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
QString TreeSolver::normalizeAbsBars(const QString& s) {
    // Turn |...| into abs(...). Handles a single level (the common case) and
    // is careful not to touch a lone '|'. We pair bars left-to-right.
    // Example: |x-3| -> abs(x-3); 2|x|+1 -> 2abs(x)+1.
    QString out;
    bool open = false;
    for (int i = 0; i < s.size(); ++i) {
        QChar ch = s[i];
        if (ch == '|') {
            if (!open) { out += "abs("; open = true; }
            else       { out += ")";    open = false; }
        } else {
            out += ch;
        }
    }
    // Unbalanced bar: give back the original so the parser reports a clean error.
    if (open) return s;
    return out;
}

QStringList TreeSolver::solve(const QString& lhsRaw, const QString& rhsRaw, QString* note) {
    QString L = normalizeAbsBars(lhsRaw);
    QString R = normalizeAbsBars(rhsRaw);

    bool okL, okR;
    NodePtr lhs = TreeParser::parse(L, okL);
    NodePtr rhs = TreeParser::parse(R, okR);
    if (!okL || !okR) return {};

    // Reject multivariable equations (this solver is single-variable).
    {
        QStringList allVars;
        for (const QString& v : TreePoly::variables(lhs)) if (!allVars.contains(v)) allVars << v;
        for (const QString& v : TreePoly::variables(rhs)) if (!allVars.contains(v)) allVars << v;
        if (allVars.size() > 1) return {};
    }

    bool handled = false;
    QStringList res;

    // Order matters: radical/abs first (they wrap the variable and would look
    // "non-polynomial" to the poly path), then rational, then plain polynomial.
    res = solveRadicalOrAbs(lhs, rhs, handled, note);
    if (handled) return res;

    res = solveRational(lhs, rhs, handled, note);
    if (handled) return res;

    res = solvePolynomial(lhs, rhs, handled);
    if (handled) return res;

    return {};   // not recognised → caller falls back
}

QStringList TreeSolver::solveEquation(const QString& equationRaw, QString* note) {
    // Split on the first '=' that isn't part of "==" etc. (simple: first '=').
    int eq = equationRaw.indexOf('=');
    if (eq < 0) return {};
    // reject if more than one '='
    if (equationRaw.indexOf('=', eq + 1) >= 0) return {};
    QString lhs = equationRaw.left(eq);
    QString rhs = equationRaw.mid(eq + 1);
    return solve(lhs, rhs, note);
}
