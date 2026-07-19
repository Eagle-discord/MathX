#pragma once
//  TreeParser — recursive-descent parser that builds an ExprTree (AST).
//
//  Grammar (same precedence/associativity as the engine's existing Expr
//  evaluator, so behaviour is consistent):
//
//    expression := addsub
//    addsub     := muldiv (('+'|'-') muldiv)*
//    muldiv     := unary  (('*'|'/') unary)*        + implicit multiplication
//    unary      := '-' unary | power
//    power      := postfix ('^' unary)?             (right-assoc)
//    postfix    := primary ('!')?                   (factorial, optional)
//    primary    := number | constant | variable
//                | function '(' args ')'
//                | '(' expression ')'
//
//  Unlike the old parser this does NOT evaluate; it returns a NodePtr. Numbers
//  are read as BigDec so no precision is lost before symbolic work happens.

#include <QString>
#include "ExprTree.h"

namespace TreeParser {
    // Parse `expr` into an AST. On success returns the root and sets ok=true.
    // On any syntax error returns nullptr with ok=false and (optionally) an
    // error message in `err`.
    NodePtr parse(const QString& expr, bool& ok, QString* err = nullptr);
}
