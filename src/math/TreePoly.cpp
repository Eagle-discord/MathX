#include "TreePoly.h"
#include <QSet>
#include <functional>
#include <algorithm>
#include <cmath>

namespace {

// A working polynomial during extraction: coefficient vector (low-order first)
// plus the variable name. Constants have an empty var and size-1 coeffs.
struct WP {
    QVector<BigDec> c{ BigDec(0) };
    QString var;          // "" for a pure constant
    bool ok = true;

    int deg() const {
        for (int i = c.size() - 1; i >= 0; --i) if (c[i] != 0) return i;
        return 0;
    }
    static WP constant(const BigDec& v) { WP w; w.c = { v }; w.var = ""; return w; }
    static WP variable(const QString& name) { WP w; w.c = { BigDec(0), BigDec(1) }; w.var = name; return w; }
    static WP fail() { WP w; w.ok = false; return w; }
};

// Reconcile the variable of two operands. A constant (empty var) adopts the
// other's var. Two different non-empty vars => not univariate => fail.
static bool unifyVar(const WP& a, const WP& b, QString& out) {
    if (a.var.isEmpty()) { out = b.var; return true; }
    if (b.var.isEmpty()) { out = a.var; return true; }
    if (a.var == b.var)  { out = a.var; return true; }
    return false;
}

static WP addWP(const WP& a, const WP& b) {
    QString v; if (!unifyVar(a, b, v)) return WP::fail();
    int n = std::max(a.c.size(), b.c.size());
    WP r; r.var = v; r.c = QVector<BigDec>(n, BigDec(0));
    for (int i = 0; i < n; ++i) {
        BigDec x = i < a.c.size() ? a.c[i] : BigDec(0);
        BigDec y = i < b.c.size() ? b.c[i] : BigDec(0);
        r.c[i] = x + y;
    }
    return r;
}
static WP subWP(const WP& a, const WP& b) {
    WP nb = b; for (BigDec& x : nb.c) x = -x; return addWP(a, nb);
}
static WP mulWP(const WP& a, const WP& b) {
    QString v; if (!unifyVar(a, b, v)) return WP::fail();
    WP r; r.var = v; r.c = QVector<BigDec>(a.c.size() + b.c.size() - 1, BigDec(0));
    for (int i = 0; i < a.c.size(); ++i)
        for (int j = 0; j < b.c.size(); ++j)
            r.c[i + j] += a.c[i] * b.c[j];
    return r;
}
static WP powWP(const WP& base, int exp) {
    WP r = WP::constant(1);
    WP b = base;
    // exponentiation by repeated multiply (exp is small in practice)
    for (int k = 0; k < exp; ++k) r = mulWP(r, b);
    return r;
}

// Try to reduce a node to a pure constant BigDec (no variable). Used for
// exponents and for detecting constant subexpressions like sqrt(2).
static bool constValue(const NodePtr& n, BigDec& out) {
    bool ok = true;
    VarBindings empty;
    BigDec v = AST::eval(n, empty, ok);
    if (ok) { out = v; return true; }
    return false;
}

static WP walk(const NodePtr& n) {
    if (!n) return WP::fail();

    switch (n->kind) {
        case NodeKind::Num: return WP::constant(n->value);

        case NodeKind::Var: {
            // constants like pi/e are numeric, not polynomial variables
            if (n->name == "pi" || n->name == "e" || n->name == "tau") {
                BigDec v; if (constValue(n, v)) return WP::constant(v);
                return WP::fail();
            }
            return WP::variable(n->name);
        }

        case NodeKind::Add: return addWP(walk(n->left), walk(n->right));
        case NodeKind::Sub: return subWP(walk(n->left), walk(n->right));
        case NodeKind::Mul: return mulWP(walk(n->left), walk(n->right));

        case NodeKind::Neg: {
            WP w = walk(n->left);
            for (BigDec& x : w.c) x = -x;
            return w;
        }

        case NodeKind::Div: {
            // Polynomial only if the divisor is a nonzero constant.
            WP denom = walk(n->right);
            if (!denom.ok) return WP::fail();
            if (!denom.var.isEmpty()) return WP::fail();      // division BY the variable => not polynomial
            if (denom.c[0] == 0) return WP::fail();           // divide by zero
            WP num = walk(n->left);
            if (!num.ok) return WP::fail();
            WP r = num; for (BigDec& x : r.c) x = x / denom.c[0];
            return r;
        }

        case NodeKind::Pow: {
            // Exponent must be a non-negative integer constant.
            BigDec ev;
            if (!constValue(n->right, ev)) return WP::fail();
            // must be integer
            BigDec rounded = BigNum::floor(ev + BigDec("0.5"));
            if (BigNum::abs(ev - rounded) > BigDec("1e-9")) return WP::fail();
            long long e = (long long)(double)rounded;
            if (e < 0) return WP::fail();

            WP base = walk(n->left);
            if (!base.ok) return WP::fail();
            // constant base ^ integer is just a constant
            if (base.var.isEmpty()) {
                BigDec bv = base.c[0];
                return WP::constant(BigNum::pow(bv, BigDec(e)));
            }
            if (e > 64) return WP::fail();   // sanity cap on expansion size
            return powWP(base, (int)e);
        }

        case NodeKind::Func: {
            // A function of the variable isn't polynomial; but a function of a
            // CONSTANT is fine and folds to a constant coefficient.
            BigDec v;
            if (constValue(n, v)) return WP::constant(v);
            return WP::fail();
        }
    }
    return WP::fail();
}

} // namespace

QStringList TreePoly::variables(const NodePtr& n) {
    QSet<QString> seen;
    std::function<void(const NodePtr&)> rec = [&](const NodePtr& x) {
        if (!x) return;
        if (x->kind == NodeKind::Var) {
            if (x->name != "pi" && x->name != "e" && x->name != "tau")
                seen.insert(x->name);
        }
        rec(x->left); rec(x->right);
        for (const auto& a : x->args) rec(a);
    };
    rec(n);
    QStringList out = seen.values();
    out.sort();
    return out;
}

UniPoly TreePoly::extract(const NodePtr& n, const QString& expectedVar) {
    UniPoly result;

    QStringList vars = variables(n);
    if (vars.size() > 1) { result.ok = false; return result; }

    WP w = walk(n);
    if (!w.ok) { result.ok = false; return result; }

    // If a specific variable was required, enforce it (unless w is constant).
    if (!expectedVar.isEmpty() && !w.var.isEmpty() && w.var != expectedVar) {
        result.ok = false; return result;
    }

    result.coeffs = w.c;
    result.var = w.var.isEmpty() ? expectedVar : w.var;
    result.ok = true;
    // trim trailing zeros
    while (result.coeffs.size() > 1 && result.coeffs.back() == 0)
        result.coeffs.pop_back();
    return result;
}
