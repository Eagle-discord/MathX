#include "ExprTree.h"
#include <QRegularExpression>
#include <cmath>

namespace AST {

NodePtr num(const BigDec& v) { auto n = std::make_shared<Node>(NodeKind::Num); n->value = v; return n; }
NodePtr num(long long v)     { auto n = std::make_shared<Node>(NodeKind::Num); n->value = BigDec(v); return n; }
NodePtr var(const QString& name) { auto n = std::make_shared<Node>(NodeKind::Var); n->name = name; return n; }

NodePtr bin(NodeKind k, NodePtr a, NodePtr b) {
    auto n = std::make_shared<Node>(k); n->left = std::move(a); n->right = std::move(b); return n;
}
NodePtr neg(NodePtr a) { auto n = std::make_shared<Node>(NodeKind::Neg); n->left = std::move(a); return n; }
NodePtr func(const QString& name, std::vector<NodePtr> args) {
    auto n = std::make_shared<Node>(NodeKind::Func); n->name = name; n->args = std::move(args); return n;
}

NodePtr clone(const NodePtr& n) {
    if (!n) return nullptr;
    auto c = std::make_shared<Node>(n->kind);
    c->value = n->value;
    c->name  = n->name;
    c->left  = clone(n->left);
    c->right = clone(n->right);
    for (const auto& a : n->args) c->args.push_back(clone(a));
    return c;
}

// ---- pretty printer -------------------------------------------------------
// Precedence for deciding when to parenthesise children.
static int prec(NodeKind k) {
    switch (k) {
        case NodeKind::Add: case NodeKind::Sub: return 1;
        case NodeKind::Mul: case NodeKind::Div: return 2;
        case NodeKind::Neg:                     return 3;
        case NodeKind::Pow:                     return 4;
        default:                                return 5;  // Num/Var/Func: atomic
    }
}

static QString fmtNum(const BigDec& v) { return BigNum::fmt(v); }

static QString toStr(const NodePtr& n, int parentPrec);

static QString wrap(const NodePtr& n, int parentPrec) {
    QString s = toStr(n, prec(n->kind));
    if (prec(n->kind) < parentPrec) return "(" + s + ")";
    return s;
}

static QString toStr(const NodePtr& n, int) {
    if (!n) return QString();
    switch (n->kind) {
        case NodeKind::Num: return fmtNum(n->value);
        case NodeKind::Var: return n->name;
        case NodeKind::Add: return wrap(n->left,1) + " + " + wrap(n->right,1);
        case NodeKind::Sub: return wrap(n->left,1) + " - " + wrap(n->right,2);
        case NodeKind::Mul: return wrap(n->left,2) + "*"  + wrap(n->right,2);
        case NodeKind::Div: return wrap(n->left,2) + "/"  + wrap(n->right,3);
        case NodeKind::Pow: return wrap(n->left,5) + "^"  + wrap(n->right,4);
        case NodeKind::Neg: return "-" + wrap(n->left,3);
        case NodeKind::Func: {
            QStringList parts;
            for (const auto& a : n->args) parts << toStr(a, 0);
            return n->name + "(" + parts.join(", ") + ")";
        }
    }
    return QString();
}

QString toString(const NodePtr& n) { return toStr(n, 0); }

// ---- numeric evaluation ---------------------------------------------------
static BigDec callFunc(const QString& name, const std::vector<BigDec>& a, bool& ok) {
    auto one = [&](auto f) -> BigDec { if (a.size() < 1) { ok = false; return 0; } return f(a[0]); };
    const BigDec DEG = BigNum::PI / 180;

    QString f = name.toLower();
    if (f == "sqrt") return one([&](const BigDec& x) -> BigDec { if (x < 0){ok=false;return BigDec(0);} return BigNum::sqrt(x); });
    if (f == "abs")  return one([&](const BigDec& x) -> BigDec { return BigNum::abs(x); });
    if (f == "cbrt") return one([&](const BigDec& x) -> BigDec { return BigDec(std::cbrt((double)x)); });
    if (f == "ln")   return one([&](const BigDec& x) -> BigDec { if (x<=0){ok=false;return BigDec(0);} return BigNum::log(x); });
    if (f == "log")  return one([&](const BigDec& x) -> BigDec { if (x<=0){ok=false;return BigDec(0);} return BigNum::log10(x); });
    if (f == "exp")  return one([&](const BigDec& x) -> BigDec { return BigNum::exp(x); });
    if (f == "sin")  return one([&](const BigDec& x) -> BigDec { return BigNum::sin(x*DEG); });
    if (f == "cos")  return one([&](const BigDec& x) -> BigDec { return BigNum::cos(x*DEG); });
    if (f == "tan")  return one([&](const BigDec& x) -> BigDec { return BigNum::tan(x*DEG); });
    if (f == "sinr") return one([&](const BigDec& x) -> BigDec { return BigNum::sin(x); });
    if (f == "cosr") return one([&](const BigDec& x) -> BigDec { return BigNum::cos(x); });
    if (f == "floor")return one([&](const BigDec& x) -> BigDec { return BigNum::floor(x); });
    if (f == "ceil") return one([&](const BigDec& x) -> BigDec { return BigNum::ceil(x); });
    if (f == "pow" && a.size() >= 2) return BigDec(BigNum::pow(a[0], a[1]));
    ok = false;
    return 0;
}

BigDec eval(const NodePtr& n, const VarBindings& vars, bool& ok) {
    if (!n) { ok = false; return 0; }
    switch (n->kind) {
        case NodeKind::Num: return n->value;
        case NodeKind::Var: {
            if (n->name == "pi")  return BigNum::PI;
            if (n->name == "e")   return BigNum::E;
            if (n->name == "tau") return 2 * BigNum::PI;
            bool got; BigDec v = vars.value(n->name, got);
            if (!got) ok = false;
            return v;
        }
        case NodeKind::Add: return eval(n->left,vars,ok) + eval(n->right,vars,ok);
        case NodeKind::Sub: return eval(n->left,vars,ok) - eval(n->right,vars,ok);
        case NodeKind::Mul: return eval(n->left,vars,ok) * eval(n->right,vars,ok);
        case NodeKind::Div: {
            BigDec d = eval(n->right,vars,ok);
            if (d == 0) { ok = false; return 0; }
            return eval(n->left,vars,ok) / d;
        }
        case NodeKind::Pow: {
            BigDec b = eval(n->left,vars,ok);
            BigDec e = eval(n->right,vars,ok);
            return BigNum::pow(b, e);
        }
        case NodeKind::Neg: return -eval(n->left,vars,ok);
        case NodeKind::Func: {
            std::vector<BigDec> av;
            for (const auto& a : n->args) av.push_back(eval(a,vars,ok));
            return callFunc(n->name, av, ok);
        }
    }
    ok = false;
    return 0;
}

} // namespace AST
