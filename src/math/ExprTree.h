#pragma once
//  ExprTree — a real expression AST for the algebra system.
//
//  This is the structural representation the engine previously lacked. The old
//  Expr evaluator folds an expression directly into a double as it parses;
//  ExprTree instead PARSES INTO A TREE that can be inspected and manipulated:
//  expanded, differentiated, rearranged, and read for exact polynomial
//  coefficients (no numeric sampling).
//
//  Design notes:
//    * Nodes are reference-counted via std::shared_ptr (aliased NodePtr) so
//      subtrees can be shared/rebuilt without ownership headaches.
//    * Values are kept as BigDec to match the precision of the rest of the
//      engine (BigNum.h). Numeric evaluation is available but is NOT how we
//      extract polynomials — that's done structurally.
//    * The tree is deliberately small and closed: five node kinds cover every
//      expression the parser accepts.

#include <QString>
#include <QStringList>
#include <memory>
#include <vector>
#include "BigNum.h"

// ---------------------------------------------------------------------------
//  Node kinds
// ---------------------------------------------------------------------------
enum class NodeKind {
    Num,     // a numeric literal            (value)
    Var,     // a variable, e.g. "x"         (name)
    Add,     // binary  a + b                (left, right)
    Sub,     // binary  a - b                (left, right)
    Mul,     // binary  a * b                (left, right)
    Div,     // binary  a / b                (left, right)
    Pow,     // binary  a ^ b                (left, right)
    Neg,     // unary   -a                   (left)
    Func     // f(args...), e.g. sqrt(x)     (name, args)
};

struct Node;
using NodePtr = std::shared_ptr<Node>;

struct Node {
    NodeKind kind;

    // Num
    BigDec value = 0;
    // Var / Func
    QString name;
    // Add/Sub/Mul/Div/Pow/Neg
    NodePtr left;
    NodePtr right;
    // Func
    std::vector<NodePtr> args;

    explicit Node(NodeKind k) : kind(k) {}

    bool isNum() const { return kind == NodeKind::Num; }
    bool isVar() const { return kind == NodeKind::Var; }
    bool isZero() const { return kind == NodeKind::Num && value == 0; }
    bool isOne()  const { return kind == NodeKind::Num && value == 1; }
};

// ---------------------------------------------------------------------------
//  Variable-binding table for AST::eval (kept separate from Qt's VarMap so this
//  header doesn't depend on the engine's double-based map). Defined BEFORE the
//  AST namespace so AST::eval can take it by reference without an incomplete-
//  type problem under strict compilers (MSVC /permissive-).
// ---------------------------------------------------------------------------
class VarBindings {
public:
    void set(const QString& name, const BigDec& v) { m_names << name; m_vals.push_back(v); }
    bool contains(const QString& name) const { return m_names.contains(name); }
    BigDec value(const QString& name, bool& ok) const {
        int i = m_names.indexOf(name);
        ok = (i >= 0);
        return ok ? m_vals[i] : BigDec(0);
    }
private:
    QStringList m_names;
    std::vector<BigDec> m_vals;
};

// ---------------------------------------------------------------------------
//  Constructors (free functions keep call sites terse)
// ---------------------------------------------------------------------------
namespace AST {
    NodePtr num(const BigDec& v);
    NodePtr num(long long v);
    NodePtr var(const QString& name);
    NodePtr bin(NodeKind k, NodePtr a, NodePtr b);
    NodePtr neg(NodePtr a);
    NodePtr func(const QString& name, std::vector<NodePtr> args);

    // Deep structural clone.
    NodePtr clone(const NodePtr& n);

    // Pretty-print back to an infix string (mainly for display/debugging).
    QString toString(const NodePtr& n);

    // Numeric evaluation with a variable binding table. Sets ok=false on any
    // unbound variable, domain error, or unknown function. This mirrors the
    // old Expr semantics (degrees for trig, etc.) closely enough for checks.
    BigDec eval(const NodePtr& n, const VarBindings& vars, bool& ok);
}
