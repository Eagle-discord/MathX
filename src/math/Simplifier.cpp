#include "Simplifier.h"
#include <QRegularExpression>
#include <QMap>
#include <QStringList>

using PolyMap = QMap<QString, double>;

static QString formatPoly(const PolyMap& poly) {
    if (poly.isEmpty()) return "0";
    QStringList keys = poly.keys();
    keys.removeAll("");
    std::sort(keys.begin(), keys.end());
    QString result;
    auto appendTerm = [&](double coeff, const QString& var) {
        if (coeff == 0.0) return;
        double abs = std::abs(coeff);
        QString sign = result.isEmpty() ? (coeff < 0 ? "-" : "") : (coeff < 0 ? "-" : "+");
        QString term;
        if (var.isEmpty())
            term = QString::number(abs, 'g', 10);
        else if (abs == 1.0)
            term = var;
        else
            term = QString::number(abs, 'g', 10) + var;
        result += sign + term;
        };
    for (const QString& k : keys) appendTerm(poly[k], k);
    if (poly.contains("")) appendTerm(poly[""], "");
    return result.isEmpty() ? "0" : result;
}

static bool parseTerm(const QString& raw, double signMul, PolyMap& out) {
    QString s = raw.trimmed();
    if (s.isEmpty()) return true;
    static QRegularExpression re(R"(^([+-]?\s*[\d.]*)?\s*([a-z]?)$)", QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(s);
    if (!m.hasMatch()) return false;
    QString coeffStr = m.captured(1).simplified().replace(" ", "");
    QString var = m.captured(2).toLower();
    double coeff = 1.0;
    if (coeffStr == "-") coeff = -1.0;
    else if (coeffStr == "+") coeff = 1.0;
    else if (!coeffStr.isEmpty()) {
        bool ok;
        coeff = coeffStr.toDouble(&ok);
        if (!ok) return false;
    }
    out[var] += coeff * signMul;
    return true;
}

struct SignedToken { double sign; QString token; };
static QVector<SignedToken> splitTopLevel(const QString& expr) {
    QVector<SignedToken> result;
    int depth = 0, start = 0;
    double sign = +1.0;
    bool first = true;
    for (int i = 0; i <= expr.size(); ++i) {
        QChar c = (i < expr.size()) ? expr[i] : QChar(0);
        if (c == '(') { depth++; continue; }
        if (c == ')') { depth--; continue; }
        bool atEnd = (i == expr.size());
        bool atSplit = (depth == 0 && (c == '+' || c == '-') && !first);
        if (atSplit || atEnd) {
            QString tok = expr.mid(start, i - start).trimmed();
            if (!tok.isEmpty()) result.append({ sign, tok });
            if (!atEnd) {
                sign = (c == '-') ? -1.0 : +1.0;
                start = i + 1;
            }
        }
        first = false;
    }
    return result;
}

static bool flattenExpr(const QString& expr, double signMul, PolyMap& out) {
    QString e = expr.trimmed();
    if (e.isEmpty()) return true;
    auto tokens = splitTopLevel(e);
    for (auto& [sign, tok] : tokens) {
        if (tok.contains('(')) {
            int open = tok.indexOf('(');
            int close = tok.lastIndexOf(')');
            if (open < 0 || close < 0) return false;
            QString pre = tok.left(open).trimmed();
            QString inner = tok.mid(open + 1, close - open - 1);
            double innerSign = sign * signMul;
            if (pre == "-") innerSign = -innerSign;
            if (!flattenExpr(inner, innerSign, out)) return false;
        }
        else {
            if (!parseTerm(tok, sign * signMul, out)) return false;
        }
    }
    return true;
}

static QString expandBrackets(const QString& input) {
    QString e = input;
    for (int pass = 0; pass < 64 && e.contains('('); ++pass) {
        int close = e.indexOf(')');
        if (close < 0) break;
        int open = e.lastIndexOf('(', close);
        if (open < 0) return {};
        QString inner = e.mid(open + 1, close - open - 1).trimmed();
        QString prefix = e.left(open);
        QString suffix = e.mid(close + 1);
        static QRegularExpression coeffRe(R"(([\d.]+)\s*$)");
        auto cm = coeffRe.match(prefix);
        double outerCoeff = 1.0;
        int coeffStart = open;
        if (cm.hasMatch()) {
            bool ok;
            outerCoeff = cm.captured(1).toDouble(&ok);
            if (ok) coeffStart = open - cm.captured(1).size();
            else outerCoeff = 1.0;
        }
        PolyMap innerPoly;
        QString innerExpanded = expandBrackets(inner);
        if (innerExpanded.isEmpty()) return {};
        if (!flattenExpr(innerExpanded, +1.0, innerPoly)) return {};
        if (outerCoeff != 1.0) {
            for (auto& v : innerPoly) v *= outerCoeff;
        }
        QString innerSimplified = formatPoly(innerPoly);
        QString newPrefix = (outerCoeff != 1.0) ? e.left(coeffStart) : prefix;
        e = newPrefix + innerSimplified + suffix;
    }
    return e;
}

QString Simplifier::simplify(const QString& expr) {
    QString e = expr.trimmed();
    e.replace('[', '(').replace(']', ')').replace('{', '(').replace('}', ')');
    // Expand brackets and collect like terms
    QString expanded = expandBrackets(e);
    if (expanded.isEmpty()) return {};
    PolyMap poly;
    if (!flattenExpr(expanded, +1.0, poly)) return {};
    for (auto it = poly.begin(); it != poly.end(); ) {
        it = (qAbs(it.value()) < 1e-12) ? poly.erase(it) : ++it;
    }
    QString simplified = formatPoly(poly);
    return (simplified == e) ? QString() : simplified;
}