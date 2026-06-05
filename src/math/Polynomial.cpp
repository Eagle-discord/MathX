#include "Polynomial.h"
#include <QRegularExpression>
bool PolynomialUtils::parse(const QString& expr, Polynomial& poly) {
    QString s = expr;
    s.remove(' ');
    s.replace("x²", "x^2");
    poly = { 0,0,0 };

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

        if (body.contains("x^2")) {
            QString coeffStr = body.left(body.indexOf("x^2"));
            if (coeffStr.isEmpty()) coeffStr = "1";
            bool ok;
            double coeff = coeffStr.toDouble(&ok);
            if (!ok) return false;
            poly.a += sign * coeff;
        }
        else if (body.contains('x')) {
            QString coeffStr = body.left(body.indexOf('x'));
            if (coeffStr.isEmpty()) coeffStr = "1";
            bool ok;
            double coeff = coeffStr.toDouble(&ok);
            if (!ok) return false;
            poly.b += sign * coeff;
        }
        else if (!body.isEmpty()) {
            bool ok;
            double coeff = body.toDouble(&ok);
            if (!ok) return false;
            poly.c += sign * coeff;
        }
    }
    return true;
}
