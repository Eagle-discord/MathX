#include "ExprTokens.h"
#include <QChar>
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
//  Tokenisation
// ---------------------------------------------------------------------------
//  We scan for runs of digits with an optional single '.' and an optional
//  exponent suffix (1e5, 2.5e-3). A leading '-' is folded into the literal ONLY
//  when it is unambiguously a sign rather than a binary minus — i.e. when the
//  previous non-space character is start-of-string, an operator, or '('.
//  Otherwise "5-3" would scrub the 3 as "-3" and produce "5--4".
// ---------------------------------------------------------------------------
void ExprTokens::retokenize(const QString& expr) {
    m_expr = expr;
    m_tokens.clear();

    const int n = expr.size();
    int i = 0;

    auto prevSignificant = [&](int pos) -> QChar {
        for (int k = pos - 1; k >= 0; --k)
            if (!expr[k].isSpace()) return expr[k];
        return QChar();     // null => start of string
        };

    while (i < n) {
        QChar c = expr[i];

        // Candidate start of a numeric literal?
        bool startsNumber = c.isDigit() || (c == '.' && i + 1 < n && expr[i + 1].isDigit());

        // A '-' begins a *signed* literal only in prefix position.
        bool signedStart = false;
        if (c == '-' && i + 1 < n &&
            (expr[i + 1].isDigit() || (expr[i + 1] == '.' && i + 2 < n && expr[i + 2].isDigit()))) {
            QChar p = prevSignificant(i);
            if (p.isNull() || p == '(' || p == '+' || p == '-' ||
                p == '*' || p == '/' || p == '^' || p == '=' || p == ',')
                signedStart = true;
        }

        if (!startsNumber && !signedStart) { ++i; continue; }

        const int start = i;
        if (signedStart) ++i;              // consume '-'

        // integer part
        while (i < n && expr[i].isDigit()) ++i;

        // fractional part
        int decimals = 0;
        if (i < n && expr[i] == '.') {
            int dot = i;
            ++i;
            int fracStart = i;
            while (i < n && expr[i].isDigit()) ++i;
            if (i == fracStart) {
                // A trailing '.' with no digits: don't consume it.
                i = dot;
            }
            else {
                decimals = i - fracStart;
            }
        }

        // exponent part: e/E followed by optional sign and >=1 digit
        if (i < n && (expr[i] == 'e' || expr[i] == 'E')) {
            int save = i;
            int j = i + 1;
            if (j < n && (expr[j] == '+' || expr[j] == '-')) ++j;
            int digStart = j;
            while (j < n && expr[j].isDigit()) ++j;
            if (j > digStart) i = j;       // valid exponent, absorb it
            else              i = save;    // lone 'e' => it's the constant, stop
        }

        const int len = i - start;
        if (len <= 0) { ++i; continue; }

        QString lit = expr.mid(start, len);
        bool ok = false;
        double v = lit.toDouble(&ok);
        if (!ok) continue;                 // shouldn't happen, but stay safe

        NumberToken t;
        t.offset = start;
        t.length = len;
        t.value = v;
        t.decimals = decimals;
        t.negative = lit.startsWith('-');
        m_tokens.push_back(t);
    }
}

// ---------------------------------------------------------------------------
//  Geometry
// ---------------------------------------------------------------------------
bool ExprTokens::tokenPixelSpan(int index, const QFontMetrics& fm,
    int& startX, int& endX) const {
    if (index < 0 || index >= m_tokens.size()) return false;
    const NumberToken& t = m_tokens[index];
    startX = fm.horizontalAdvance(m_expr.left(t.offset));
    endX = fm.horizontalAdvance(m_expr.left(t.end()));
    return true;
}

int ExprTokens::hitTest(int pixelX, const QFontMetrics& fm, int maxDistancePx) const {
    if (m_tokens.isEmpty()) return -1;

    int best = -1;
    int bestDist = std::numeric_limits<int>::max();

    for (int k = 0; k < m_tokens.size(); ++k) {
        int a, b;
        tokenPixelSpan(k, fm, a, b);

        int dist;
        if (pixelX < a)      dist = a - pixelX;
        else if (pixelX > b) dist = pixelX - b;
        else                 dist = 0;          // inside the token

        if (dist < bestDist) { bestDist = dist; best = k; }
        if (dist == 0) break;                   // exact hit, stop early
    }

    if (maxDistancePx >= 0 && bestDist > maxDistancePx) return -1;
    return best;
}

// ---------------------------------------------------------------------------
//  Rewriting
// ---------------------------------------------------------------------------
QString ExprTokens::formatLike(int index, double value) const {
    if (index < 0 || index >= m_tokens.size())
        return QString::number(value, 'g', 10);

    const NumberToken& t = m_tokens[index];
    if (t.decimals > 0)
        return QString::number(value, 'f', t.decimals);

    // Integer-looking token: keep it integral if the value is (near-)integral,
    // otherwise fall back to a compact representation so we never silently
    // truncate a value the user can see changing.
    double rounded = std::round(value);
    if (std::abs(value - rounded) < 1e-9)
        return QString::number(static_cast<long long>(rounded));
    return QString::number(value, 'g', 10);
}

QString ExprTokens::withValueAt(int index, double newValue) const {
    if (index < 0 || index >= m_tokens.size()) return m_expr;
    const NumberToken& t = m_tokens[index];

    QString rendered = formatLike(index, newValue);

    // If the replacement is negative and the character immediately before the
    // token is a binary operator, wrap in parens so "5 * -3" stays valid rather
    // than becoming "5 * -3" -> fine, but "5 - -3" reads badly. Parenthesising
    // is always safe and unambiguous.
    if (rendered.startsWith('-') && !t.negative) {
        int p = t.offset - 1;
        while (p >= 0 && m_expr[p].isSpace()) --p;
        if (p >= 0) {
            QChar pc = m_expr[p];
            if (pc == '-' || pc == '+' || pc == '*' || pc == '/' || pc == '^')
                rendered = "(" + rendered + ")";
        }
    }

    QString out = m_expr;
    out.replace(t.offset, t.length, rendered);
    return out;
}

double ExprTokens::stepFor(int index) const {
    if (index < 0 || index >= m_tokens.size()) return 1.0;
    const NumberToken& t = m_tokens[index];

    // Precision-driven step: a token typed with 2 decimals moves in 0.01 units.
    if (t.decimals > 0)
        return std::pow(10.0, -t.decimals);

    // Integer token: scale the step with magnitude so large numbers move
    // usefully fast (1200 -> steps of 10) but small ones stay precise.
    double mag = std::abs(t.value);
    if (mag >= 10000) return 100.0;
    if (mag >= 1000)  return 10.0;
    if (mag >= 100)   return 1.0;
    return 1.0;
}