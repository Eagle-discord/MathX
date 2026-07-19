#include "Polynomial.h"
#include "../math/BigNum.h"
#include <QRegularExpression>
#include <QDebug>

bool PolynomialUtils::parse(const QString& expr, Polynomial& poly) {
    QString s = expr;
    s.remove(' ');
    s.replace("x²", "x^2");
    poly = Polynomial{ 0, 0, 0 };  // a, b, c as BigDec

    // Split into terms by '+' or '-', preserving sign
    QList<QString> terms;
    QString current;
    for (int i = 0; i < s.size(); ++i) {
        QChar ch = s[i];
        if ((ch == '+' || ch == '-') && !current.isEmpty()) {
            terms.append(current);
            current = ch;
        }
        else {
            current += ch;
        }
    }
    if (!current.isEmpty()) terms.append(current);

    for (const QString& term : terms) {
        double sign = 1.0;
        QString body = term;
        if (term.startsWith('-')) {
            sign = -1.0;
            body = term.mid(1);
        }
        else if (term.startsWith('+')) {
            body = term.mid(1);
        }

        // Determine which coefficient it is
        QString coeffStr;
        if (body.contains("x^2")) {
            coeffStr = body.left(body.indexOf("x^2"));
            if (coeffStr.isEmpty()) coeffStr = "1";
            BigDec coeff;
            try {
                coeff = BigDec(coeffStr.toStdString());
            }
            catch (...) {
                qDebug() << "Failed to parse coefficient:" << coeffStr;
                return false;
            }
            poly.a += (sign == 1.0 ? coeff : -coeff);
        }
        else if (body.contains('x')) {
            coeffStr = body.left(body.indexOf('x'));
            if (coeffStr.isEmpty()) coeffStr = "1";
            BigDec coeff;
            try {
                coeff = BigDec(coeffStr.toStdString());
            }
            catch (...) {
                qDebug() << "Failed to parse coefficient:" << coeffStr;
                return false;
            }
            poly.b += (sign == 1.0 ? coeff : -coeff);
        }
        else if (!body.isEmpty()) {
            // constant term
            BigDec coeff;
            try {
                coeff = BigDec(body.toStdString());
            }
            catch (...) {
                qDebug() << "Failed to parse constant:" << body;
                return false;
            }
            poly.c += (sign == 1.0 ? coeff : -coeff);
        }
    }
    return true;
}