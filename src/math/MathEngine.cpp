#include "MathEngine.h"
#include "BigNum.h"
#include "Expr.h"
#include "Solver.h"
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <set>
#include "shapes/ShapeDef.h"



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
        if (pos < s.size() && (s[pos] == '+' || s[pos] == '-') && n.isEmpty()) {}
        while (pos < s.size() && s[pos].isDigit()) n += s[pos++];
        if (pos < s.size() && s[pos] == '.') {
            n += s[pos++];
            while (pos < s.size() && s[pos].isDigit()) n += s[pos++];
        }
        if (pos < s.size() && (s[pos] == 'e' || s[pos] == 'E')) {
            n += s[pos++];
            if (pos < s.size() && (s[pos] == '+' || s[pos] == '-')) n += s[pos++];
            while (pos < s.size() && s[pos].isDigit()) n += s[pos++];
        }
        return n;
    }
};

static double evalExpr(Tok2& t, const VarMap& vars);
static double evalTerm(Tok2& t, const VarMap& vars);
static double evalPow(Tok2& t, const VarMap& vars);
static double evalUnary(Tok2& t, const VarMap& vars);
static double evalPrimary(Tok2& t, const VarMap& vars);

// ----------------------------------------------------------------------
//  Helper: format a double for display
// ----------------------------------------------------------------------
static QString fmt(double v) {
    if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
    if (std::isnan(v)) return "Undefined";
    return QString::number(v, 'g', 10);
}

// ----------------------------------------------------------------------
//  Public evaluation entry points (delegate to Expr and BigNum)
// ----------------------------------------------------------------------
double MathEngine::evalSimple(const QString& input, bool& ok) {
    return Expr::eval(input, ok);
}
double MathEngine::evalWith(const QString& input, const VarMap& vars, bool& ok) {
    return Expr::evalWith(input, vars, ok);
}

QString MathEngine::bigEval(const QString& input, bool& ok) {
    return BigNum::bigEval(input, ok);
}


QString MathEngine::bigFactorial(int n) {
    return BigNum::bigFactorial(n);
}

// ----------------------------------------------------------------------
//  Number theory helpers
// ----------------------------------------------------------------------
long long MathEngine::gcd(long long a, long long b) {
    a = std::abs(a); b = std::abs(b);
    while (b) { a %= b; std::swap(a, b); }
    return a;
}

long long MathEngine::lcm(long long a, long long b) {
    return std::abs(a / gcd(a, b) * b);
}
long long MathEngine::nCr(int n, int r) {
    if (r < 0 || r > n) return 0;
    if (r == 0 || r == n) return 1;

    r = std::min(r, n - r);
    long long res = 1;

    for (int i = 1; i <= r; ++i) {

        res = res * (n - i + 1) / i;
    }

    return res;
}
long long MathEngine::nPr(int n, int r) {
    if (r < 0 || r > n) return 0;
    long long res = 1;
    for (int i = 0; i < r; ++i) res *= (n - i);
    return res;
}

static QString normaliseBrackets(QString s) {
    s.replace('[', '('); s.replace(']', ')');
    s.replace('{', '('); s.replace('}', ')');
    s.replace(QChar(0x2329), '('); s.replace(QChar(0x232A), ')');  // angle brackets
    s.replace(QChar(0x27E8), '('); s.replace(QChar(0x27E9), ')');  // math angle brackets
    s.replace(QChar(0x2308), '('); s.replace(QChar(0x2309), ')');  // ceil
    s.replace(QChar(0x230A), '('); s.replace(QChar(0x230B), ')');  // floor
    return s;
}

// ----------------------------------------------------------------------
//  tryConversion – unit conversions
// ----------------------------------------------------------------------
CalcResult MathEngine::tryConversion(const QString& expr) {
    static QRegularExpression re(R"(^([\d.eE+\-]+)\s+(\w+)\s+to\s+(\w+)$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(expr.trimmed());
    if (!m.hasMatch()) return {};
    double val = m.captured(1).toDouble();
    QString from = m.captured(2).toLower(), to = m.captured(3).toLower();

    QMap<QString, double> lenToM = {
        {"m",1},{"km",1000},{"cm",0.01},{"mm",0.001},
        {"miles",1609.344},{"mile",1609.344},{"ft",0.3048},{"feet",0.3048},
        {"in",0.0254},{"inch",0.0254},{"yd",0.9144}
    };
    QMap<QString, double> massToKg = {
        {"kg",1},{"g",0.001},{"lbs",0.453592},{"lb",0.453592},
        {"oz",0.0283495},{"t",1000}
    };
    QMap<QString, double> dataToB = {
        {"b",1},{"kb",1024},{"mb",1024 * 1024.0},
        {"gb",1024 * 1024 * 1024.0},{"tb",1024 * 1024 * 1024 * 1024.0}, {"pb", 1024 * 1024 * 1024 * 1024 * 1024}
    };
    QMap<QString, double> speedToMs = {
        {"m/s",1},{"km/h",1 / 3.6},{"kph",1 / 3.6},
        {"mph",0.44704},{"knot",0.514444}
    };
    QMap<QString, double> volToL = {
        {"l",1},{"ml",0.001},{"gallon",3.78541},{"gallons",3.78541},
        {"gal",3.78541},{"cup",0.236588},{"fl_oz",0.0295735}
    };
    

    auto tryConv = [&](QMap<QString, double>& tbl) -> QString {
        if (!tbl.contains(from) || !tbl.contains(to)) return {};
        return QString("%1 %2 = %3 %4").arg(val).arg(from)
            .arg(fmt(val * tbl[from] / tbl[to])).arg(to);
        };

    QString r;
    if (!(r = tryConv(lenToM)).isEmpty())    return { r,"conv" };
    if (!(r = tryConv(massToKg)).isEmpty())  return { r,"conv" };
    if (!(r = tryConv(dataToB)).isEmpty())   return { r,"conv" };
    if (!(r = tryConv(speedToMs)).isEmpty()) return { r,"conv" };
    if (!(r = tryConv(volToL)).isEmpty())    return { r,"conv" };

    // Temperature
    if ((from == "f" || from == "fahrenheit") && (to == "c" || to == "celsius"))
        return { QString("%1°F = %2°C").arg(val).arg(fmt((val - 32) * 5.0 / 9.0)), "conv" };
    if ((from == "c" || from == "celsius") && (to == "f" || to == "fahrenheit"))
        return { QString("%1°C = %2°F").arg(val).arg(fmt(val * 9.0 / 5.0 + 32)), "conv" };
    if ((from == "c" || from == "celsius") && (to == "k" || to == "kelvin"))
        return { QString("%1°C = %2 K").arg(val).arg(fmt(val + 273.15)), "conv" };
    if ((from == "k" || from == "kelvin") && (to == "c" || to == "celsius"))
        return { QString("%1 K = %2°C").arg(val).arg(fmt(val - 273.15)), "conv" };
    if (from == "f" && to == "k")
        return { QString("%1°F = %2 K").arg(val).arg(fmt((val - 32) * 5.0 / 9.0 + 273.15)), "conv" };
    return {};
}

// ----------------------------------------------------------------------
//  tryGeometry – basic geometric calculations (static, not interactive)
// ----------------------------------------------------------------------
static QMap<QString, double> parseParams(const QString& s) {
    QMap<QString, double> p;
    static QRegularExpression re(R"((\w+)\s*=\s*([\d.eE+\-]+))");
    auto it = re.globalMatch(s);
    while (it.hasNext()) {
        auto m = it.next();
        p[m.captured(1).toLower()] = m.captured(2).toDouble();
    }
    return p;
}


CalcResult MathEngine::tryGeometry(const QString& expr) {
    QString e = expr.trimmed().toLower();
    // Extract the base command: everything before '(' or first space
    QString base = e.split('(').first().split(' ').first();

    for (const Shape& shape : ALL_SHAPES) {
        if (base == shape.canonical || shape.aliases.contains(base)) {
            // This is a known shape – delegate to the interactive card system
            return { "", "geo" };
        }
    }
    return {};
}
// ----------------------------------------------------------------------
//  tryTrig – trigonometric evaluation (delegates to Expr)
// ----------------------------------------------------------------------
CalcResult MathEngine::tryTrig(const QString& expr) {
    static QStringList fns = { "sin","cos","tan","asin","acos","atan","atan2",
                              "sinr","cosr","tanr","asinr","acosr","atanr" };
    QString lo = expr.toLower();
    bool hasTrig = false;
    for (auto& f : fns) if (lo.contains(f)) { hasTrig = true; break; }
    if (!hasTrig) return {};
    if (expr.contains('=')) return {};  // equation, not a plain trig eval
    bool ok;
    double v = Expr::eval(expr, ok);
    if (!ok) return {};
    return { fmt(v), "trig" };
}
static QRegularExpression shapePattern() {
    static const QStringList shapes = {
        "circ", "circle", "tri", "triangle", "rect", "rectangle",
        "sqre", "square", "sphr", "sphere", "cyld", "cylinder",
        "cone", "cube", "cuboid", "tetra", "tetrahedron", "octa", "octahedron",
        "icosa", "icosahedron", "dodeca", "dodecahedron", "prism", "pyramid",
        "torus", "ellipsoid", "capsule", "ellipse", "righttri", "parallel",
        "trap", "rhombus", "regpoly", "sector", "annulus", "hemi", "hollcyl", "frustum"
    };
    // Match: start, optional whitespace, one of the shapes,
    // optional parentheses, then either '=', or whitespace + letter + '='
    QString pattern = "^\\s*(" + shapes.join("|") + ")(?:\\s*\\([^)]*\\))?\\s*([=]|\\s+[a-z]\\s*=)";
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption);
}

double MathEngine::evalSide(const QString& side, const QString& varName, double val) {
    VarMap vm;
    vm[varName] = val;
    bool ok;
    double result = Expr::evalWith(side, vm, ok);
    if (!ok) throw std::runtime_error("Evaluation failed");
    return result;
}

// ── Polynomial simplifier v3 ──────────────────────────────────────────────────
//
// Pipeline:
//   1. expandBrackets(expr)  — recursively simplifies all bracketed
//      sub-expressions first, substituting each with its flat result.
//      e.g. "2(x + x)" → "2(2x)" → "4x"
//           "x + (3x - x)" → "x + 2x"
//
//   2. flattenExpr(expanded) — splits on top-level +/- and collects
//      like terms into a PolyMap.
//
//   3. formatPoly(poly)      — renders the PolyMap back to a string.
//
// Supported:
//   - arbitrary nesting of parentheses
//   - numeric coefficient before parens: 2(x+x), -3(x+1)
//   - +/- of terms, multi-term expressions
//   - coefficients: 2x, -x, 0.5x, x, -x (implicit ±1)
//   - constants mixed with variables: 2x + 3 - x
//   - single-variable only (x^2, xy rejected cleanly)
// ─────────────────────────────────────────────────────────────────────────────

using PolyMap = QMap<QString, double>;  // key="" → constant term

// ── formatPoly ────────────────────────────────────────────────────────────────
static QString formatPoly(const PolyMap& poly) {
    if (poly.isEmpty()) return "0";

    QStringList keys = poly.keys();
    keys.removeAll("");
    std::sort(keys.begin(), keys.end());

    QString result;
    auto appendTerm = [&](double coeff, const QString& var) {
        if (coeff == 0.0) return;
        double abs = std::abs(coeff);
        QString sign = result.isEmpty()
            ? (coeff < 0 ? "-" : "")
            : (coeff < 0 ? " - " : " + ");
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
    if (poly.contains(""))        appendTerm(poly[""], "");

    return result.isEmpty() ? "0" : result;
}

// ── parseTerm ─────────────────────────────────────────────────────────────────
// Parses a single flat term (no parens): "2x", "-x", "3", "0.5x"
// Returns false for anything it can't handle (x^2, xy, etc.)
static bool parseTerm(const QString& raw, double signMul, PolyMap& out) {
    QString s = raw.trimmed();
    if (s.isEmpty()) return true;   // empty is fine (skip)

    static QRegularExpression re(
        R"(^([+-]?\s*[\d.]*)?\s*([a-z]?)$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(s);
    if (!m.hasMatch()) return false;

    QString coeffStr = m.captured(1).simplified().replace(" ", "");
    QString var = m.captured(2).toLower();

    double coeff = 1.0;
    if (coeffStr == "-")       coeff = -1.0;
    else if (coeffStr == "+")  coeff = 1.0;
    else if (!coeffStr.isEmpty()) {
        bool ok;
        coeff = coeffStr.toDouble(&ok);
        if (!ok) return false;
    }

    out[var] += coeff * signMul;
    return true;
}


// ── Term parser ───────────────────────────────────────────────────────────────
// Parses a flat (no-parens) term like "2x", "-x", "3", "0.5x"
// Returns false if the term contains anything unexpected (e.g. x^2, xy)
static bool parseTerm(const QString& raw, PolyMap& out) {
    QString s = raw.trimmed();
    if (s.isEmpty()) return false;

    // Extract leading sign/coefficient
    static QRegularExpression termRe(
        R"(^([+-]?\s*[\d.]*)?\s*([a-z]?)$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = termRe.match(s);
    if (!m.hasMatch()) return false;

    QString coeffStr = m.captured(1).simplified().replace(" ", "");
    QString var = m.captured(2).toLower();

    double coeff = 1.0;
    if (!coeffStr.isEmpty() && coeffStr != "+" && coeffStr != "-") {
        bool ok;
        coeff = coeffStr.toDouble(&ok);
        if (!ok) return false;
    }
    else if (coeffStr == "-") {
        coeff = -1.0;
    }

    out[var] += coeff;
    return true;
}
// ── splitTopLevel ─────────────────────────────────────────────────────────────
// Splits `expr` on top-level (depth-0) `+` and `-`.
// Returns list of {sign, token} pairs where sign is +1.0 or -1.0.
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
            if (!tok.isEmpty())
                result.append({ sign, tok });
            if (!atEnd) {
                sign = (c == '-') ? -1.0 : +1.0;
                start = i + 1;
            }
        }
        first = false;
    }
    return result;
}


// ── Expression flattener ──────────────────────────────────────────────────────
// Recursively expands parentheses and collects terms into a PolyMap.
// 'sign' is the current multiplier (+1 or -1) inherited from the context.
// Forward declaration
static bool flattenExpr(const QString& expr, double signMul, PolyMap& out);
// ── flattenExpr ───────────────────────────────────────────────────────────────
// Splits on top-level +/- and accumulates into a PolyMap.
// Assumes brackets have already been expanded (no parens in tokens).
static bool flattenExpr(const QString& expr, double signMul, PolyMap& out) {
    QString e = expr.trimmed();
    if (e.isEmpty()) return true;

    auto tokens = splitTopLevel(e);
    for (auto& [sign, tok] : tokens) {
        // If a token still has parens (nested), recurse
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


// ── tryFraction ───────────────────────────────────────────────────────────────
static QString tryFraction(const QString& e) {
    static QRegularExpression re(R"(^(-?\d+)\s*/\s*(-?\d+)$)");
    auto m = re.match(e);
    if (!m.hasMatch()) return {};
    long long num = m.captured(1).toLongLong();
    long long den = m.captured(2).toLongLong();
    if (den == 0) return {};
    long long a = std::abs(num), b = std::abs(den);
    while (b) { a %= b; std::swap(a, b); }
    long long g = a;
    if (g <= 1) return {};
    long long rn = num / g, rd = den / g;
    if (rd < 0) { rn = -rn; rd = -rd; }
    return (rd == 1) ? QString::number(rn)
        : QString("%1/%2").arg(rn).arg(rd);
}

// ── tryConstantFold ───────────────────────────────────────────────────────────
static QString tryConstantFold(const QString& e) {
    static QRegularExpression re(
        R"(^([\d.]+)\s*\*\s*(pi|e)\s*\*\s*1$)"
        R"(|^1\s*\*\s*(pi|e)\s*\*\s*([\d.]+)$)"
        R"(|^([\d.]+)\s*\*\s*1\s*\*\s*(pi|e)$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(e);
    if (!m.hasMatch()) return {};
    QString coeff, constName;
    for (int i = 1; i <= 6; ++i) {
        if (m.captured(i).isEmpty()) continue;
        QString cap = m.captured(i).toLower();
        if (cap == "pi" || cap == "e") constName = cap;
        else coeff = cap;
    }
    if (coeff.isEmpty() || constName.isEmpty()) return {};
    return coeff + ((constName == "pi") ? "\xcf\x80" : "e");
}
// ── expandBrackets ────────────────────────────────────────────────────────────
// Finds the innermost parenthesised group, simplifies it, and substitutes
// the result back as a flat string. Repeats until no parens remain.
// Returns the fully expanded string, or empty string on failure.
static QString expandBrackets(const QString& input) {
    QString e = input;

    // Safety limit — prevent infinite loops on malformed input
    for (int pass = 0; pass < 64 && e.contains('('); ++pass) {

        // Find the innermost ( ... ) — the first ')' and scan left for its '('
        int close = e.indexOf(')');
        if (close < 0) break;
        int open = e.lastIndexOf('(', close);
        if (open < 0) return {};  // unmatched paren

        QString inner = e.mid(open + 1, close - open - 1).trimmed();
        QString prefix = e.left(open);         // everything before '('
        QString suffix = e.mid(close + 1);     // everything after ')'

        // Check if there's a numeric coefficient immediately before '('
        // e.g. "2(x+x)" → prefix ends with "2"
        static QRegularExpression coeffRe(R"(([\d.]+)\s*$)");
        auto cm = coeffRe.match(prefix);
        double outerCoeff = 1.0;
        int coeffStart = open;  // where the coefficient starts in e
        if (cm.hasMatch()) {
            bool ok;
            outerCoeff = cm.captured(1).toDouble(&ok);
            if (ok) coeffStart = open - cm.captured(1).size();
            else    outerCoeff = 1.0;
        }

        // Simplify the inner expression into a PolyMap
        PolyMap innerPoly;
        QString innerExpanded = expandBrackets(inner);  // recurse fully
        if (innerExpanded.isEmpty()) return {};
        if (!flattenExpr(innerExpanded, +1.0, innerPoly)) return {};

        // Scale the inner poly by outerCoeff
        if (outerCoeff != 1.0) {
            for (auto& v : innerPoly) v *= outerCoeff;
        }

        // Format the simplified inner back to a string
        QString innerSimplified = formatPoly(innerPoly);

        // Reconstruct: prefix (without the coefficient) + innerSimplified + suffix
        QString newPrefix = (outerCoeff != 1.0)
            ? e.left(coeffStart)
            : prefix;

        e = newPrefix + innerSimplified + suffix;
    }

    return e;
}
// ── trySimplify — main entry point ────────────────────────────────────────────
static QString trySimplify(const QString& expr) {
    QString e = expr.trimmed();
    // Normalize all bracket types to ()
    e.replace('[', '('); e.replace(']', ')');
    e.replace('{', '('); e.replace('}', ')');


    // 1. Fraction reduction (no variables)
    QString frac = tryFraction(e);
    if (!frac.isEmpty()) return frac;

    // 2. Constant folding (pi/e)
    QString fold = tryConstantFold(e);
    if (!fold.isEmpty()) return fold;

    // 3. Polynomial simplification
    if (!Expr::detectVariables(e.toLower()).isEmpty()) {

        // Phase A: expand all brackets first
        QString expanded = expandBrackets(e);
        if (expanded.isEmpty()) return {};   // parse failure

        // Phase B: collect like terms
        PolyMap poly;
        if (!flattenExpr(expanded, +1.0, poly)) return {};

        // Remove zero-coefficient terms
        for (auto it = poly.begin(); it != poly.end(); )
            it = (it.value() == 0.0) ? poly.erase(it) : ++it;

        QString simplified = formatPoly(poly);

        // Only return if something actually changed
        if (simplified != e && !simplified.isEmpty())
            return simplified;
    }

    return {};
}

// ----------------------------------------------------------------------
//  tryAlgebra – equation solving using Solver
// ----------------------------------------------------------------------
CalcResult MathEngine::tryAlgebra(const QString& expr) {
    static QRegularExpression shapeRe = shapePattern();
    if (shapeRe.match(expr).hasMatch()) {
        // It's a known shape with parameters → let geometry handle it
        return tryGeometry(expr);
    }
    QString e = expr.trimmed();
    if (!e.contains('=')) return {};

    auto vars = Expr::detectVariables(e.toLower());
    if (vars.isEmpty()) return {};

    QStringList sides = e.split('=');
    if (sides.size() != 2) return {};
    QString lhs = sides[0].trimmed(), rhs = sides[1].trimmed();

    // Try quadratic detection
    QString varName = *vars.begin();
    if (vars.size() == 1) {
        QRegularExpression q2(
            QString(R"(([+-]?\s*[\d.]*)\s*%1\^2\s*([+-]\s*[\d.]*)\s*%1\s*([+-]\s*[\d.]*))")
            .arg(QRegularExpression::escape(varName)),
            QRegularExpression::CaseInsensitiveOption);
        auto q2m = q2.match(lhs);
        if (q2m.hasMatch() && rhs.trimmed() == "0") {
            double a = q2m.captured(1).simplified().replace(" ", "").toDouble();
            if (a == 0) a = 1;
            double b = q2m.captured(2).simplified().replace(" ", "").toDouble();
            double c = q2m.captured(3).simplified().replace(" ", "").toDouble();
            return { Solver::solveQuadratic(a,b,c).join("\n"), "alg" };
        }
    }

    // ── Fast exact linear solver ──────────────────────────────────────────────
    // For ax + b = 0, f(0)=b and f(1)=a+b, so a=f(1)-f(0), b=f(0).
    // We verify linearity by checking f(2) == 2a+b.
    // This handles x+5=2x-3, 4x-7=x+8, 3x+10=2x+25 exactly in O(1).
    if (vars.size() == 1) {
        QString combined = lhs + "-(" + rhs + ")";
        VarMap vm0, vm1, vm2;
        vm0[varName] = 0.0; vm1[varName] = 1.0; vm2[varName] = 2.0;
        bool ok0, ok1, ok2;
        double f0 = Expr::evalWith(combined, vm0, ok0);
        double f1 = Expr::evalWith(combined, vm1, ok1);
        double f2 = Expr::evalWith(combined, vm2, ok2);
        if (ok0 && ok1 && ok2) {
            double a = f1 - f0;
            double b = f0;
            // Linearity check: f(2) must equal 2a+b
            if (std::abs(f2 - (2.0 * a + b)) < 1e-9) {
                if (std::abs(a) < 1e-12)
                    return { std::abs(b) < 1e-9
                        ? "Infinite solutions" : "No solution", "err" };
                double x = -b / a;
                double rounded = std::round(x);
                if (std::abs(x - rounded) < 1e-9) x = rounded;
                return { QString("%1 = %2").arg(varName).arg(fmt(x)), "alg" };
            }
        }
    }

    // ── Newton solver (non-linear fallback) ───────────────────────────────────
    if (vars.size() == 1) {
        bool ok;
        double result = Solver::solveLinear(lhs, rhs, varName, ok);
        if (ok) {
            double rounded = std::round(result);
            if (std::abs(result - rounded) < 1e-9) result = rounded;
            return { QString("%1 = %2").arg(varName).arg(fmt(result)), "alg" };
        }
        return { "Could not solve — no real solution found", "err" };
    }

    QStringList varList;
    for (auto& v : vars) varList << v;
    return { QString("Multiple variables: %1\nProvide values for all but one").arg(varList.join(", ")), "err" };
}

// ----------------------------------------------------------------------
//  tryArithmetic – fallback for numeric expressions
// ----------------------------------------------------------------------
CalcResult MathEngine::tryArithmetic(const QString& expr) {

    // ── "X of Y" — fraction/percentage of a number ───────────────────────────
 // Handles: "1/2 of 400" → 200
 //          "3/4 of 80"  → 60
 //          "20% of 300" → 60  (bonus: percent form)
    {
        /*std::string neString;
        QString newString = QString(neString.c_str());
        
        if (expr.contains(R"(^[\(\[\{].*[\)\]\}]$)")) {
            std::string str = expr.toStdString();
            
            for (int i = 0; i < str.length(); i++) {
                if ((str[i] == '{' || str[i] == '}') || (str[i] == '(' || str[i] == ')') || (str[i] == '[' || str[i] == ']')) {
                    continue;
                }
                newString.push_back(str[i]);
            }
        }*/
        static QRegularExpression ofRe(
            R"(^([\d./%]+)\s*of\s*([\d.]+)$)",
            QRegularExpression::CaseInsensitiveOption);
        auto m = ofRe.match(expr.trimmed());
        if (m.hasMatch()) {
            QString fracPart = m.captured(1).trimmed();
            double rhs = m.captured(2).toDouble();
            double lhs = 0;
            bool ok = false;

            // Handle percentage: "20%"
            if (fracPart.contains('%')) {
                lhs = fracPart.chopped(1).toDouble(&ok) / 100.0;
            }
            // Handle fraction: "1/2"
            else if (fracPart.contains('/')) {
                QStringList parts = fracPart.split('/');
                if (parts.size() == 2) {
                    double num = parts[0].toDouble(&ok);
                    double den = parts[1].toDouble();
                    if (ok && den != 0) lhs = num / den;
                    else ok = false;
                }
            }
            // Handle plain decimal: "0.5"
            else {
                lhs = fracPart.toDouble(&ok);
            }

            if (ok) {
                double result = lhs * rhs;
                return { fmt(result), "ok" };
            }
        }
    }

    // ── Common measurement conversions (natural language) ────────────────────
// Handles: "1 furlong", "3 furlongs in meters", "2 nautical miles"
// These are fixed-unit lookups — no "to" keyword needed
    {
       

        static const QVector<UnitDef> units = {
            { {"furlong","furlongs"},           201.168,    "meters"   },
            { {"nautical mile","nautical miles",
               "nauticalmile","nauticalmiles",
               "nmi"},                          1852.0,     "meters"   },
            { {"light year","light years",
               "lightyear","lightyears","ly"},  9.461e15,   "meters"   },
            { {"parsec","parsecs","pc"},         3.086e16,   "meters"   },
            { {"fathom","fathoms"},              1.8288,     "meters"   },
            { {"league","leagues"},              4828.032,   "meters"   },
            { {"rod","rods"},                    5.0292,     "meters"   },
            { {"chain","chains"},                20.1168,    "meters"   },
            { {"hand","hands"},                  0.1016,     "meters"   },
            { {"cubit","cubits"},                0.4572,     "meters"   },
            { {"acre","acres"},                  4046.856,   "meters²"  },
            { {"hectare","hectares","ha"},        10000.0,    "meters²"  },
            { {"pint","pints","pt"},              0.473176,   "Liters"   },
            { {"quart","quarts","qt"},            0.946353,   "Liters"   },
            { {"fluid ounce","fl oz","floz"},     0.0295735,  "Liters"   },
            { {"stone","stones","st"},            6.35029,    "kilograms"  },
            { {"tonne","tonnes","metric ton"},    1000.0,     "kilograms"  },
            { {"mph"},                            0.44704,    "meters/second" },
            { {"knot","knots","kn"},              0.514444,   "meters/second" },
            { {"kilometers", "km", "kilometres"}, 1000, "meters"},
            {{"feet", "ft"}, 0.3048, "meters"},
            
        };

        static QRegularExpression measRe(
            R"(^([\d.]+)\s+(.+)$)",
            QRegularExpression::CaseInsensitiveOption);
        auto mm = measRe.match(expr.trimmed());
        if (mm.hasMatch()) {
            double val = mm.captured(1).toDouble();
            QString unit = mm.captured(2).trimmed().toLower();
            // Strip trailing "in meters/in km/etc" if present
            static QRegularExpression inRe(R"(\s+in\s+\w+$)",
                QRegularExpression::CaseInsensitiveOption);
            unit.remove(inRe);
            unit = unit.trimmed();

            for (const UnitDef& ud : units) {
                for (const QString& name : ud.names) {
                    if (unit == name.toLower()) {
                        double si = val * ud.toMeters;
                        return {
                            QString("%1 %2 = %3 %4")
                                .arg(val).arg(name)
                                .arg(fmt(si)).arg(ud.siUnit),
                            "conv"
                        };
                    }
                }
            }
        }
    }

    // Try high‑precision evaluation first
    bool bok;
    QString bigResult = bigEval(expr, bok);
    if (bok) {
        bool ok;
        double dbl = evalSimple(expr, ok);
        QString dblStr = ok ? fmt(dbl) : "";
        if (bigResult.length() > dblStr.length() + 3)
            return { bigResult, "big" };
        if (ok) return { dblStr, "ok" };
        return { bigResult, "ok" };
    }

    // Fallback to double evaluation
    bool ok;
    double v = evalSimple(expr, ok);
    if (!ok) return {};
    return { fmt(v), "ok" };
}

bool MathEngine::solveEquation(const QString& eq, double& result) {
    // Remove outer parentheses if present
    QString s = eq.trimmed();
    if (s.startsWith('(') && s.endsWith(')')) {
        s = s.mid(1, s.length() - 2).trimmed();
    }
    // Must contain '='
    if (!s.contains('=')) return false;
    // Split into left and right sides
    QStringList sides = s.split('=');
    if (sides.size() != 2) return false;
    QString lhs = sides[0].trimmed();
    QString rhs = sides[1].trimmed();

    // Detect a single variable (a letter, not 'e')
    QRegularExpression varRe(R"(\b([a-df-z])\b)");
    auto match = varRe.match(lhs + rhs);
    if (!match.hasMatch()) return false;
    QString varName = match.captured(1);

    bool ok;
    double val = Solver::solveLinear(lhs, rhs, varName, ok);
    if (ok) {
        result = val;
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------
//  Main dispatcher
// ----------------------------------------------------------------------
CalcResult MathEngine::evaluate(const QString& expr) {
    if (expr.startsWith("f(") && expr.endsWith(")")) throw std::runtime_error("Unknown Function 'f'");
    if (expr.trimmed().isEmpty()) throw std::runtime_error("Empty expression");
    CalcResult r;
    r = tryConversion(expr);  if (!r.type.isEmpty()) return r;
    r = tryGeometry(expr);    if (!r.type.isEmpty()) return r;
    r = tryTrig(expr);        if (!r.type.isEmpty()) return r;
    r = tryAlgebra(expr);     if (!r.type.isEmpty()) return r;
    r = tryArithmetic(expr);  if (!r.type.isEmpty()) return r;
    throw std::runtime_error("Cannot parse expression");
}