#pragma once
#include <QString>
#include <QMap>
#include <QSet>
#include "BigNum.h"


using VarMap = QMap<QString, double>;

namespace Expr {
    double eval(const QString& expr, bool& ok);
    double evalWith(const QString& expr, const VarMap& vars, bool& ok);
    QSet<QString> detectVariables(const QString& expr);
    QString preprocess(const QString& expr);
    void computeFactorialAsync(BigInt n);
}