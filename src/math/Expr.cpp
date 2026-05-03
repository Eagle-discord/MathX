#include "Expr.h"
#include <QRegularExpression>
#include <QSet>
#include <stdexcept>
#include <cmath>
#include <limits>
#include "MathEngine.h"


// ── Bracket normaliser ────────────────────────────────────────────────────────
static QString normaliseBrackets(QString s) {
    s.replace('[', '('); s.replace(']', ')');
    s.replace('{', '('); s.replace('}', ')');
    s.replace(QChar(0x2329), '('); s.replace(QChar(0x232A), ')');
    s.replace(QChar(0x27E8), '('); s.replace(QChar(0x27E9), ')');
    s.replace(QChar(0x2308), '('); s.replace(QChar(0x2309), ')');
    s.replace(QChar(0x230A), '('); s.replace(QChar(0x230B), ')');
    return s;
}

// ── Implicit multiply pre-processor ──────────────────────────────────────────
static QString insertImplicitMul(const QString& s) {
    QString r;
    for (int i = 0; i < s.size(); ++i) {
        r += s[i];
        if (i + 1 < s.size()) {
            QChar c = s[i], n = s[i + 1];
            if ((c.isDigit() || c == ')') && (n.isLetter() || n == '('))
                r += '*';
            else if (c.isLetter() && n.isDigit())
                r += '*';
        }
    }


    return r;
}

// ── Tokeniser and parser for double expressions ───────────────────────────────
struct Tok2 {
    QString s; int pos = 0;
    void skip() { while (pos < s.size() && s[pos].isSpace()) ++pos; }
    bool atEnd() { skip(); return pos >= s.size(); }
    QChar peek() { skip(); return pos < s.size() ? s[pos] : QChar(0); }
    QChar get() { skip(); return pos < s.size() ? s[pos++] : QChar(0); }
    QString readWord() {
        skip(); QString w;
        while (pos < s.size() && (s[pos].isLetterOrNumber() || s[pos] == '_'))
            w += s[pos++];
        return w;
    }
    QString readNumber() {
        skip(); QString n;
        if (peek() == '+' || peek() == '-') n += get();
        while (peek().isDigit()) n += get();
        if (peek() == '.') { n += get(); while (peek().isDigit()) n += get(); }
        if (peek().toLower() == 'e') {
            n += get();
            if (peek() == '+' || peek() == '-') n += get();
            while (peek().isDigit()) n += get();
        }
        return n;
    }
};

static double evalExpr(Tok2& t, const VarMap& vars);
static double evalTerm(Tok2& t, const VarMap& vars);
static double evalPow(Tok2& t, const VarMap& vars);
static double evalUnary(Tok2& t, const VarMap& vars);
static double evalPrimary(Tok2& t, const VarMap& vars);

static double callBuiltin(const QString& name, const QVector<double>& args) {
    auto need = [&](int n) {
        if (args.size() < n) throw std::runtime_error(name.toStdString() + " needs " + std::to_string(n) + " argument(s)");
        };
    auto d2r = [](double d) { return d * M_PI / 180.0; };
    auto r2d = [](double r) { return r * 180.0 / M_PI; };

    if (name == "sin") { need(1); return std::sin(d2r(args[0])); }
    if (name == "cos") { need(1); return std::cos(d2r(args[0])); }
    if (name == "tan") { need(1); return std::tan(d2r(args[0])); }
    if (name == "asin") { need(1); return r2d(std::asin(args[0])); }
    if (name == "acos") { need(1); return r2d(std::acos(args[0])); }
    if (name == "atan") { need(1); return r2d(std::atan(args[0])); }
    if (name == "atan2") { need(2); return r2d(std::atan2(args[0], args[1])); }
    if (name == "sinr") { need(1); return std::sin(args[0]); }
    if (name == "cosr") { need(1); return std::cos(args[0]); }
    if (name == "tanr") { need(1); return std::tan(args[0]); }
    if (name == "asinr") { need(1); return std::asin(args[0]); }
    if (name == "acosr") { need(1); return std::acos(args[0]); }
    if (name == "atanr") { need(1); return std::atan(args[0]); }
    if (name == "sqrt") { need(1); if (args[0] < 0) throw std::runtime_error("sqrt of negative"); return std::sqrt(args[0]); }
    if (name == "cbrt") { need(1); return std::cbrt(args[0]); }
    if (name == "abs") { need(1); return std::abs(args[0]); }
    if (name == "log") { need(1); if (args[0] <= 0) throw std::runtime_error("log non-positive"); return std::log10(args[0]); }
    if (name == "ln") { need(1); if (args[0] <= 0) throw std::runtime_error("ln non-positive"); return std::log(args[0]); }
    if (name == "log2") { need(1); return std::log2(args[0]); }
    if (name == "exp") { need(1); return std::exp(args[0]); }
    if (name == "floor") { need(1); return std::floor(args[0]); }
    if (name == "ceil") { need(1); return std::ceil(args[0]); }
    if (name == "round") { need(1); return std::round(args[0]); }
    if (name == "sign") { need(1); return (args[0] > 0) - (args[0] < 0); }
    if (name == "min") { need(2); return std::min(args[0], args[1]); }
    if (name == "max") { need(2); return std::max(args[0], args[1]); }
    if (name == "pow") { need(2); return std::pow(args[0], args[1]); }
    if (name == "mod") { need(2); return std::fmod(args[0], args[1]); }
    if (name == "hypot") { need(2); return std::hypot(args[0], args[1]); }
    if (name == "fact") {
        need(1);
        throw std::runtime_error("__BIGFACT__:" + std::to_string((long long)args[0]));
    }
    if (name == "lcm") {
        need(2);
        long long a = std::abs((long long)args[0]), b = std::abs((long long)args[1]);
        if (a == 0 || b == 0) return 0;
        long long ta = a, tb = b;
        while (tb) { ta %= tb; std::swap(ta, tb); }
        return (double)(a / ta * b);
    }
    if (name == "gcd" || name == "hcf") {
        need(2);
        long long a = std::abs((long long)args[0]), b = std::abs((long long)args[1]);
        while (b) { a %= b; std::swap(a, b); }
        return (double)a;
    }
    if (name == "ncr" || name == "c") {
        need(2);
        int n = (int)args[0], r = (int)args[1];
        if (r < 0 || r > n) return 0;
        r = std::min(r, n - r);
        long long res = 1;
        for (int i = 0; i < r; ++i) res = res * (n - i) / (i + 1);
        return (double)res;
    }
    if (name == "npr") {
        need(2);
        int n = (int)args[0], r = (int)args[1];
        if (r < 0 || r > n) return 0;
        long long res = 1;
        for (int i = 0; i < r; ++i) res *= (n - i);
        return (double)res;
    }

    // Unknown function — friendly error, not a raw exception
    throw std::runtime_error("Unknown function '" + name.toStdString() + "'");
}

static double evalPrimary(Tok2& t, const VarMap& vars) {
    t.skip();
    QChar c = t.peek();
    if (c == '(') {
        t.get();
        double v = evalExpr(t, vars);
        t.skip();
        if (t.peek() == ')') t.get();
        else throw std::runtime_error("Missing ')'");
        // Postfix % after parenthesised expression: (50+50)% → 1.0
        if (t.peek() == '%') { t.get(); v /= 100.0; }
        // Postfix ! after parenthesised expression: (5)! → fact(5)
        if (t.peek() == '!') {
            t.get();
            throw std::runtime_error("__BIGFACT__:" + std::to_string((long long)v));
        }
        return v;
    }
    if (c.isDigit() || c == '.') {
        QString n = t.readNumber();
        double v = n.toDouble();
        // Postfix % : 20% → 0.2
        if (t.peek() == '%') { t.get(); v /= 100.0; }
        // Postfix ! : 5! → fact(5)
        if (t.peek() == '!') {
            t.get();
            throw std::runtime_error("__BIGFACT__:" + std::to_string((long long)v));
        }
        return v;
    }
    if (c.isLetter()) {
        int savedPos = t.pos;
        QString word = t.readWord();
        QString wl = word.toLower();
        t.skip();
        // Constants
        if (wl == "pi") return M_PI;
        if (wl == "e") return M_E;
        if (wl == "tau") return 2.0 * M_PI;
        if (wl == "inf") return std::numeric_limits<double>::infinity();
        // Function call
        if (t.peek() == '(') {
            t.get();
            QVector<double> args;
            if (t.peek() != ')') {
                args.append(evalExpr(t, vars));
                while (t.peek() == ',') { t.get(); args.append(evalExpr(t, vars)); }
            }
            t.skip();
            if (t.peek() != ')') throw std::runtime_error("Missing ')' after " + wl.toStdString());
            t.get();
            return callBuiltin(wl, args);
        }
        // Variable
        if (vars.contains(wl)) return vars[wl];
        // Single-char fallback
        if (word.size() > 1) {
            QString first = word.left(1).toLower();
            if (vars.contains(first)) {
                t.pos = savedPos + 1;
                return vars[first];
            }
        }
        throw std::runtime_error("Unknown identifier: " + word.toStdString());
    }
    throw std::runtime_error("Unexpected character");
}

static double evalUnary(Tok2& t, const VarMap& vars) {
    t.skip();
    if (t.peek() == '-') { t.get(); return -evalUnary(t, vars); }
    if (t.peek() == '+') { t.get(); return evalUnary(t, vars); }
    return evalPrimary(t, vars);
}

static double evalPow(Tok2& t, const VarMap& vars) {
    double base = evalUnary(t, vars);
    t.skip();
    if (t.peek() == '^' || (t.peek() == '*' && t.pos + 1 < t.s.size() && t.s[t.pos + 1] == '*')) {
        if (t.peek() == '*') { t.get(); t.get(); }
        else t.get();
        double exp = evalPow(t, vars);
        base = std::pow(base, exp);
    }
    return base;
}

static double evalTerm(Tok2& t, const VarMap& vars) {
    double v = evalPow(t, vars);
    while (true) {
        t.skip();
        QChar op = t.peek();
        if (op != '*' && op != '/' && op != '%') break;
        if (op == '*' && t.pos + 1 < t.s.size() && t.s[t.pos + 1] == '*') break; // not power
        t.get();
        double r = evalPow(t, vars);
        if (op == '*') v *= r;
        else if (op == '/') { if (r == 0) throw std::runtime_error("Division by zero"); v /= r; }
        else v = std::fmod(v, r);
    }
    return v;
}

static double evalExpr(Tok2& t, const VarMap& vars) {
    double v = evalTerm(t, vars);
    while (true) {
        t.skip();
        QChar op = t.peek();
        if (op != '+' && op != '-') break;
        t.get();
        double r = evalTerm(t, vars);
        v = (op == '+') ? v + r : v - r;
    }
    return v;
}

// ── Public interface implementations ──────────────────────────────────────────
double Expr::eval(const QString& input, bool& ok) {
    return evalWith(input, {}, ok);
}

double Expr::evalWith(const QString& input, const VarMap& vars, bool& ok) {
    try {
        QString s = input.simplified();
        s = normaliseBrackets(s);
        s = s.replace("**", "^");
        s = insertImplicitMul(s);
        Tok2 t;
        t.s = s;
        t.pos = 0;
        double v = evalExpr(t, vars);
        t.skip();
        if (!t.atEnd()) { ok = false; return 0; }
        ok = true;
        return v;
    }
    catch (const std::exception&) {
        ok = false;
        return 0;
    }
}

QSet<QString> Expr::detectVariables(const QString& expr) {
    QSet<QString> vars;
    static const QSet<QString> exclude = {
        "pi","e","tau","inf","sin","cos","tan","asin","acos","atan","atan2",
        "sinr","cosr","tanr","asinr","acosr","atanr","sqrt","cbrt","abs",
        "log","ln","log2","exp","floor","ceil","round","sign","min","max",
        "pow","mod","hypot","fact","ncr","npr","gcd","lcm"
    };
    QRegularExpression re(R"([a-df-z])"); // a-d, f-z (exclude 'e')
    auto it = re.globalMatch(expr.toLower());
    while (it.hasNext()) {
        auto m = it.next();
        QString v = m.captured(0);
        if (!exclude.contains(v)) vars.insert(v);
    }
    return vars;
}

QString Expr::preprocess(const QString& expr) {
    QString s = expr.simplified().replace("**", "^");
    s = normaliseBrackets(s);
    return insertImplicitMul(s);
}