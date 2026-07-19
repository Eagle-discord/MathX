#include "MathEngine.h"
#include "BigNum.h"
#include "Expr.h"
#include "Solver.h"
#include <thread/PersistentWorker.h>
#include "Algebra.h"
#include "PolyEngine.h"
#include "TreeSolver.h"
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <set>
#include "shapes/ShapeDef.h"
#include "NaturalLanguage.h"
#include "Simplifier.h"
#include <limits>

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

// Forward declarations for user-defined-function expansion (defined lower down,
// but referenced earlier by tryFactor and the main dispatcher).
static QString     expandUserCalls(const QString& expr, int depth = 0);
static QStringList splitArgs(const QString& s);
static bool        substituteBody(const FunctionDef& def, const QStringList& args, QString& out);
static QString     substituteVariables(const QString& expr);

// ----------------------------------------------------------------------
//  Helper: format a double for display
// ----------------------------------------------------------------------
static QString fmt(double v) {
    if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
    if (std::isnan(v)) return "Undefined";
    return QString::number(v, 'g', 10);
}

// ----------------------------------------------------------------------
//  Names reserved by the engine itself — a user function can't be defined
//  with one of these, so "sin(x) = ..." etc. never shadows a builtin.
// ----------------------------------------------------------------------
static const QSet<QString>& builtinConstantNames() {
    static const QSet<QString> names = { "pi", "e", "tau", "inf" };
    return names;
}
static const QSet<QString>& builtinFunctionNames() {
    static const QSet<QString> names = {
        "sin","cos","tan","sinh","cosh","tanh","asin","acos","atan",
        "asinh","acosh","atanh","atan2","sinr","cosr","tanr","asinr","acosr","atanr",
        "sqrt","cbrt","abs","log","ln","log2","exp","floor","ceil","round","sign",
        "min","max","pow","mod","hypot","fact","gcd","hcf","lcm","ncr","npr","c","logbase"
    };
    return names;
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
    if (a == 0 || b == 0) return 0;
    const long long g = gcd(a, b);
    return std::abs(a / g * b);
}
long long MathEngine::nCr(int n, int r) {
    if (r < 0 || r > n) return 0;
    if (r == 0 || r == n) return 1;
    r = std::min(r, n - r);
    long long res = 1;
    for (int i = 1; i <= r; ++i) {
        // Divide first where possible to keep the partial product small, and
        // bail out to a big-int path rather than silently overflowing.
        const long long mul = n - i + 1;
        if (res > std::numeric_limits<long long>::max() / mul)
            throw std::overflow_error("nCr too large — result exceeds 64-bit range");
        res = res * mul / i;
    }
    return res;
}
long long MathEngine::nPr(int n, int r) {
    if (r < 0 || r > n) return 0;
    long long res = 1;
    for (int i = 0; i < r; ++i) {
        const long long mul = n - i;
        if (mul != 0 && res > std::numeric_limits<long long>::max() / mul)
            throw std::overflow_error("nPr too large — result exceeds 64-bit range");
        res *= mul;
    }
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
            return { "", ResultType::geo };
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
    return { fmt(v), ResultType::trig };
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

using PolyMap = QMap<QString, double>;  // key="" → constant term

// -- formatPoly ----------------------------------------------------------------
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

// -- parseTerm -----------------------------------------------------------------
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


// -- Term parser ---------------------------------------------------------------
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
// -- splitTopLevel -------------------------------------------------------------
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



static bool flattenExpr(const QString& expr, double signMul, PolyMap& out);

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


// -- tryFraction ---------------------------------------------------------------
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

// -- tryConstantFold -----------------------------------------------------------
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
// -- expandBrackets ------------------------------------------------------------
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
// -- trySimplify — main entry point --------------------------------------------
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


static QString replaceExponentiation(const QString& expr, bool& cancelled) {
    QString result = expr;
    // Pattern: integer ^ integer (with optional spaces)
    static QRegularExpression powRe(R"((\d+)\s*\^\s*(\d+))");
    int offset = 0;
    while (true) {
        auto match = powRe.match(result, offset);
        if (!match.hasMatch()) break;
        QString baseStr = match.captured(1);
        QString expStr = match.captured(2);
        BigInt base(baseStr.toStdString());
        BigInt exp(expStr.toStdString());
        // Compute using bigPow (with cancellation)
        try {
            auto callback = [](int) {

                };
            QString powResult = BigNum::bigPow(base, exp, callback, &PersistentWorker::s_cancel);
            // Replace the matched text with the result (parenthesised)
            int start = match.capturedStart();
            int end = match.capturedEnd();
            result.replace(start, end - start, "(" + powResult + ")");
            offset = start + powResult.length() + 2; // move past replacement
        }
        catch (const std::exception& e) {
            cancelled = true;
            return {};
        }
    }
    return result;
}
// Formats a number with thousands separators for the verification result.
// e.g. 10000 → "10,000"   3.14159 → "3.14159"
static QString fmtVerified(double v) {
    if (std::isinf(v)) return v > 0 ? "Infinity" : "-Infinity";
    if (std::isnan(v)) return "Undefined";

    // Check if it's a whole number so we can add commas
    if (v == std::floor(v) && std::abs(v) < 1e15) {
        long long iv = static_cast<long long>(v);
        QString s = QString::number(std::abs(iv));
        // Insert commas every 3 digits from the right
        for (int i = s.length() - 3; i > 0; i -= 3)
            s.insert(i, ',');
        return (iv < 0 ? "-" : "") + s;
    }
    return QString::number(v, 'g', 10);
}

// BigDec counterpart of fmtVerified, for the equality-verification display.
// Exists because the double version saturates: "200! = 1" printed
// "Infinity ≠ 1" and "2^1000 = 2^1000" compared inf to inf and announced them
// equal. BigNum::fmtFull already handles NaN/Inf, expands scientific notation,
// and collapses past kMaxExpandDigits (1000), so this only adds the digit
// grouping that fmtVerified does -- and only when the result is short enough
// that commas help rather than bloat.
static QString fmtVerifiedBig(const BigDec& v) {
    namespace bmp = boost::multiprecision;
    if (bmp::isnan(v)) return QStringLiteral("Undefined");
    if (bmp::isinf(v)) return v.sign() < 0 ? QStringLiteral("-Infinity")
        : QStringLiteral("Infinity");

    QString s = BigNum::fmtFull(v);

    // Group only plain integers, and only modest ones. A 375-digit 200! with
    // commas every three places is 125 extra characters of noise; past ~30
    // digits the grouping stops being an aid to reading.
    static const QRegularExpression plainInt(R"(^-?\d+$)");
    if (!plainInt.match(s).hasMatch()) return s;

    const bool neg = s.startsWith('-');
    QString digits = neg ? s.mid(1) : s;
    if (digits.size() > 30) return s;

    for (int i = digits.size() - 3; i > 0; i -= 3)
        digits.insert(i, ',');
    return (neg ? QStringLiteral("-") : QString()) + digits;
}

// ── Helper: evaluate an expression with a variable substituted using BigDec ──
static bool bigEvalSubstitution(const QString& expr, const QString& varName, const QString& valueStr, BigDec& result) {
    QString substituted = expr;
    // Use word boundaries to avoid replacing substrings (e.g., "x" in "exp")
    QRegularExpression varRe("\\b" + QRegularExpression::escape(varName) + "\\b");
    substituted.replace(varRe, "(" + valueStr + ")");
    // Preprocess the substituted expression to handle implicit multiplication, etc.
    QString processed = Expr::preprocess(substituted);
    bool ok;
    QString resStr = BigNum::bigEval(processed, ok);
    if (!ok) return false;
    try {
        result = BigDec(resStr.toStdString());
    }
    catch (...) {
        return false;
    }
    return true;
}

// ----------------------------------------------------------------------
//  tryAlgebra – equation solving using Solver
// ----------------------------------------------------------------------

CalcResult MathEngine::tryAlgebra(const QString& expr) {
    static QRegularExpression shapeRe = shapePattern();
    if (shapeRe.match(expr).hasMatch()) {
        return tryGeometry(expr);
    }
    QString e = expr.trimmed();
    if (!e.contains('=')) return {};

    QStringList sides = e.split('=');
    if (sides.size() != 2) return {};
    QString lhs = sides[0].trimmed(), rhs = sides[1].trimmed();

    // ── Detect variables ──────────────────────────────────────────────────────
    QSet<QString> vars = Expr::detectVariables(lhs + " " + rhs);

    // A multi-letter "variable" here is almost always a stray word ("then",
    // "find", "if") rather than a solvable unknown — real stored variables were
    // already substituted to numbers upstream. Decline so we never confidently
    // "solve" garbage like "x = 45 then ..." into "then = 8.68e-05".
    for (const QString& v : vars) if (v.size() > 1) return {};

    bool hasVars = !vars.isEmpty();

    if (!hasVars) {
        // ── Equality verification (no variables) ──────────────────────────
        // Evaluated in BigDec, not double. tryArithmetic already prefers
        // bigEvalValue for the same reason: the double path saturates. "200! = 1"
        // used to reach Expr::eval, overflow to inf, and either refuse to parse
        // or compare infinities; "2^1000 = 2^1000" compared inf to inf and
        // proudly announced they were equal. BigDec evaluates both sides exactly
        // and the comparison is meaningful at any magnitude.
        //
        // Note bigEvalValue's own fallback: if BigDec can't parse a side (it has
        // no user-function support, for instance), we drop to Expr::eval for that
        // side rather than declining outright, so nothing that used to work stops
        // working.
        bool lhsOk = false, rhsOk = false;
        BigDec lhsBig = BigNum::bigEvalValue(lhs, lhsOk);
        BigDec rhsBig = BigNum::bigEvalValue(rhs, rhsOk);

        double lhsVal = 0.0, rhsVal = 0.0;
        bool bigPath = lhsOk && rhsOk;
        if (!bigPath) {
            lhsOk = rhsOk = false;
            lhsVal = Expr::eval(lhs, lhsOk);
            rhsVal = Expr::eval(rhs, rhsOk);
            if (!lhsOk || !rhsOk) return {};
        }

        bool equal;
        QString lhsStr, rhsStr;
        if (bigPath) {
            // Exact compare, with a relative tolerance for the irrational cases
            // (sqrt(2)*sqrt(2) = 2 must still pass). An absolute epsilon can't
            // work here: these values range from 1e-30 to 1e2568.
            const BigDec diff = lhsBig > rhsBig ? lhsBig - rhsBig : rhsBig - lhsBig;
            const BigDec absL = lhsBig < 0 ? -lhsBig : lhsBig;
            const BigDec absR = rhsBig < 0 ? -rhsBig : rhsBig;
            const BigDec mag = absL > absR ? absL : absR;
            equal = (diff == 0) || (mag > 0 && diff / mag < BigDec("1e-40"));
            lhsStr = fmtVerifiedBig(lhsBig);
            rhsStr = fmtVerifiedBig(rhsBig);
        }
        else {
            const double epsilon = 1e-10;
            equal = std::abs(lhsVal - rhsVal) < epsilon;
            lhsStr = fmtVerified(lhsVal);
            rhsStr = fmtVerified(rhsVal);
        }

        // Symmetric TRUE/FALSE reports: both spell out the LHS and RHS values so
        // a failing check shows *why* it failed, not just that it did.
        QString result;
        if (equal) {
            result = QString("LHS (Left-hand Side) = %1\n"
                "RHS (Right Hand Side) = %2\n"
                "%1 = %2  ✓\n"
                "LHS = RHS, Hence Proved")
                .arg(lhsStr, rhsStr);
        }
        else {
            result = QString("LHS (Left-hand Side) = %1\n"
                "RHS (Right Hand Side) = %2\n"
                "%1 ≠ %2  ✗\n"
                "LHS ≠ RHS, Not Equal")
                .arg(lhsStr, rhsStr);
        }
        return { result, ResultType::ok };
    }

    // ── Algebraic equation — try high‑precision linear solver ──────────────
    if (vars.size() == 1) {
        QString varName = *vars.begin();

        // ── AST-based solver: handles the richer equation forms that the
        // sampling paths can't — brackets like (x-1)(x-2)(x-3)=0, both-sides
        // polynomials, rational equations x/(x-2)=3 (with excluded values),
        // and radical/abs equations sqrt(x+1)=4, |x-3|=5 (with extraneous-root
        // checks). If it declines (empty result), we fall through to the
        // original linear/quadratic/cubic paths below.
        {
            QString note;
            QStringList treeRoots = TreeSolver::solve(lhs, rhs, &note);
            if (!treeRoots.isEmpty()) {
                QString out = treeRoots.join("\n");
                if (!note.isEmpty()) out += "\n(" + note + ")";
                return { out, ResultType::alg };
            }
        }

        QString combined = lhs + "-(" + rhs + ")";

        // Only for linear equations (no '^', no 'x²', no function calls)
        if (!combined.contains('^') && !combined.contains("x²") && !combined.contains("x^2") &&
            !combined.contains("sin") && !combined.contains("cos") && !combined.contains("log")) {
            BigDec f0, f1;
            if (bigEvalSubstitution(combined, varName, "0", f0) &&
                bigEvalSubstitution(combined, varName, "1", f1)) {
                BigDec a = f1 - f0;
                BigDec b = f0;
                if (BigNum::abs(a) < BigDec("1e-12")) {
                    if (BigNum::abs(b) < BigDec("1e-12"))
                        return { "Infinite solutions", ResultType::err };
                    else
                        return { "No solution", ResultType::err };
                }
                BigDec x = -b / a;
                // Round to nearest integer if very close
                double xd = static_cast<double>(x);
                double rounded = std::round(xd);
                if (std::abs(xd - rounded) < 1e-9)
                    xd = rounded;
                return { QString("%1 = %2").arg(varName).arg(BigNum::fmt(x)), ResultType::alg };
            }
        }
    }

    // ── Fallback to existing algebra solver (quadratic, cubic+, etc.) ──────
    QString solution = Algebra::solveEquation(lhs, rhs);
    if (solution.isEmpty()) return {};
    return { solution, ResultType::alg };
}

// ----------------------------------------------------------------------
//  tryFactor — "factor(x^2 - 5x + 6)" or "factorise x^2-9"
//  Recognises an explicit factor/factorise/factorize keyword and hands the
//  inner polynomial to Algebra::factor.
// ----------------------------------------------------------------------
CalcResult MathEngine::tryFactor(const QString& expr) {
    static QRegularExpression re(
        R"(^\s*factor(?:ise|ize)?\s*\(?\s*(.+?)\s*\)?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(expr);
    if (!m.hasMatch()) return {};

    QString body = m.captured(1).trimmed();
    if (body.isEmpty()) return {};

    // If the body references a user-defined function (e.g. "factor(f(x))"),
    // expand it to the underlying expression first.
    {
        static const QRegularExpression idParen(R"(([A-Za-z][A-Za-z0-9_]*)\s*\()");
        auto it = idParen.globalMatch(body);
        bool refsUserFn = false;
        while (it.hasNext())
            if (isFunctionDefined(it.next().captured(1).toLower())) { refsUserFn = true; break; }
        if (refsUserFn) body = expandUserCalls(body);
    }

    QString factored = Algebra::factor(body);
    if (factored.isEmpty())
        return { "Cannot factor (irreducible or not a polynomial)", ResultType::err };

    // Show it as an equivalence: original = factored form.
    Poly p;
    QString original = body;
    if (PolyEngine::parse(Expr::preprocess(body), p))
        original = p.toString();
    return { QString("%1 = %2").arg(original, factored), ResultType::alg, factored };
}

// ----------------------------------------------------------------------
//  trySystem — a system of linear equations.
//  Accepts multiple equations separated by ';', ',', or newlines, e.g.
//    "x + y = 5; x - y = 1"      "2a+b=5, a-b=1"
//  Requires at least two '=' signs so single equations still go to tryAlgebra.
// ----------------------------------------------------------------------
CalcResult MathEngine::trySystem(const QString& expr) {
    QString e = expr.trimmed();
    if (e.count('=') < 2) return {};   // not a system

    // Split on ';', newline, or ',' — but only treat as separators between
    // complete equations. Simple split works because each piece must contain '='.
    static QRegularExpression sep(R"([;\n,])");
    QStringList rawParts = e.split(sep, Qt::SkipEmptyParts);
    if (rawParts.size() < 2) return {};

    QStringList equations;
    for (QString part : rawParts) {
        part = part.trimmed();
        if (part.isEmpty()) continue;
        if (!part.contains('=')) return {};   // a fragment without '=' => not a clean system
        equations << part;
    }
    if (equations.size() < 2) return {};

    QString solution = Algebra::solveSystem(equations);
    if (solution.isEmpty()) return {};
    return { solution, ResultType::alg };
}

// ----------------------------------------------------------------------
//  User-defined functions:  f(x) = x + 5      P(x) = 5x^2 + 1
//  Defining one stores it; afterwards f(3), P(-2), f(2*x+1), etc. all work.
// ----------------------------------------------------------------------
static QMap<QString, FunctionDef>& functionRegistry() {
    static QMap<QString, FunctionDef> reg;   // keyed by lower-cased name
    return reg;
}

// name(params) = body   e.g. "f(x) = x + 5", "g(x, y) = x^2 + y"
// The parameter list captured in group 2 is split on commas by the handler.
static const QRegularExpression& funcDefPattern() {
    static const QRegularExpression re(
        R"(^\s*([A-Za-z][A-Za-z0-9_]*)\s*\(\s*([A-Za-z][A-Za-z0-9_]*(?:\s*,\s*[A-Za-z][A-Za-z0-9_]*)*)\s*\)\s*=\s*(.+?)\s*$)");
    return re;
}

// Split a function-argument string on top-level commas (depth-0), so that
// "3, x+1" -> {"3", "x+1"} but "atan2(1,2), 3" keeps the inner comma grouped.
static QStringList splitArgs(const QString& s) {
    QStringList out;
    int depth = 0, start = 0;
    for (int i = 0; i < s.size(); ++i) {
        QChar c = s[i];
        if (c == '(') ++depth;
        else if (c == ')') --depth;
        else if (c == ',' && depth == 0) {
            out << s.mid(start, i - start).trimmed();
            start = i + 1;
        }
    }
    out << s.mid(start).trimmed();
    return out;
}

// Given a call to a user function, produce the substituted body:
// bind each parameter to its argument (wrapped in parens so precedence is
// safe), using word-boundary replacement so "x" in the body isn't matched
// inside "exp" or another identifier. Returns false on arity mismatch.
static bool substituteBody(const FunctionDef& def, const QStringList& args, QString& out) {
    if (args.size() != def.arity()) return false;
    QString body = def.body;

    // Replace all parameters "simultaneously" by first renaming each to a
    // unique placeholder, then swapping placeholders for the actual arguments.
    // This prevents an argument that contains another parameter's name from
    // being re-substituted (e.g. g(y, x) called as g(x, 1) must not turn the
    // injected x into the second round's target).
    for (int i = 0; i < def.params.size(); ++i) {
        QRegularExpression pe("\\b" + QRegularExpression::escape(def.params[i]) + "\\b");
        body.replace(pe, QString("\x01%1\x02").arg(i));   // sentinel-wrapped index
    }
    for (int i = 0; i < args.size(); ++i) {
        body.replace(QString("\x01%1\x02").arg(i), "(" + args[i] + ")");
    }
    out = body;
    return true;
}

// Recursively expand every user-defined function call inside `expr`, replacing
// each with its substituted body. This is what makes calls compose: nested
// calls f(g(x)) expand inner-first, and calls embedded in larger expressions
// like 2*f(3)+1 expand in place. Builtins (sin, sqrt, ...) are left untouched
// so the normal evaluator handles them.
//
// `depth` guards against runaway recursion (e.g. a definition that calls
// itself). Returns the expanded string; on depth overflow returns the input
// unchanged so the caller can still try to evaluate it.
static QString expandUserCalls(const QString& expr, int depth) {
    if (depth > 64) return expr;      // recursion / mutual-recursion safety net

    // Find identifiers immediately followed by '('. Scan left-to-right,
    // rebuilding the string with expansions applied. We re-scan from scratch
    // after any expansion so newly-revealed calls are caught.
    static const QRegularExpression idParen(R"(([A-Za-z][A-Za-z0-9_]*)\s*\()");

    auto it = idParen.globalMatch(expr);
    while (it.hasNext()) {
        auto m = it.next();
        QString name = m.captured(1);
        QString lname = name.toLower();

        const FunctionDef* def = MathEngine::functionDef(lname);
        if (!def) continue;           // not a user function (builtin or unknown)

        // Locate the matching ')' for the '(' this match ends on.
        int openParen = m.capturedEnd() - 1;   // index of '('
        int p = openParen, dep = 0, close = -1;
        for (; p < expr.size(); ++p) {
            if (expr[p] == '(') ++dep;
            else if (expr[p] == ')') { if (--dep == 0) { close = p; break; } }
        }
        if (close < 0) break;         // unbalanced — let evaluator report it

        QString inside = expr.mid(openParen + 1, close - openParen - 1);

        // Expand any calls *within* the arguments first (inner-first / composition).
        QString innerExpanded = expandUserCalls(inside, depth + 1);
        QStringList args = splitArgs(innerExpanded);

        QString substituted;
        if (!substituteBody(*def, args, substituted)) {
            // Arity mismatch: leave this occurrence alone. It'll surface as an
            // evaluation error later, which is the right signal to the user.
            continue;
        }

        // Rebuild: prefix + (expanded body) + suffix, then recurse on the whole
        // thing so the freshly-inserted body's own calls (if any) are expanded.
        int nameStart = m.capturedStart();
        QString prefix = expr.left(nameStart);
        QString suffix = expr.mid(close + 1);
        QString rebuilt = prefix + "(" + substituted + ")" + suffix;
        return expandUserCalls(rebuilt, depth + 1);
    }

    return expr;    // no user calls left
}

CalcResult MathEngine::tryFunctionDefinition(const QString& expr) {
    auto m = funcDefPattern().match(expr);
    if (!m.hasMatch()) return {};

    QString name = m.captured(1);
    QString paramList = m.captured(2);
    QString body = m.captured(3).trimmed();
    QString lname = name.toLower();

    // Never let a definition shadow a builtin constant/function or a shape name
    if (builtinConstantNames().contains(lname) || builtinFunctionNames().contains(lname))
        return {};
    for (const Shape& shape : ALL_SHAPES) {
        if (lname == shape.canonical || shape.aliases.contains(lname)) return {};
    }

    // Parse and validate parameters.
    QStringList params;
    for (const QString& raw : paramList.split(',')) {
        QString p = raw.trimmed();
        if (p.isEmpty()) return {};
        // A parameter can't reuse a reserved constant name (e.g. "f(pi) = ...").
        if (builtinConstantNames().contains(p.toLower())) return {};
        // Duplicate parameters (e.g. "f(x, x)") are rejected.
        if (params.contains(p)) return {};
        params << p;
    }
    if (params.isEmpty()) return {};

    // Make sure the body actually evaluates with all parameters bound — this
    // rejects garbage like "f(x) = )(" while accepting "x^2 + y" etc. We also
    // expand any user-function calls inside the body first, so a definition
    // like "h(x) = f(x) + 1" (referring to an already-defined f) validates.
    QString bodyExpanded = expandUserCalls(body);
    bool ok;
    VarMap testVars;
    for (const QString& p : params) testVars[p] = 1.0;
    Expr::evalWith(bodyExpanded, testVars, ok);
    if (!ok) return {};

    FunctionDef def;
    def.name = name;
    def.params = params;
    def.body = body;
    def.display = QString("%1(%2) = %3").arg(name, params.join(", "), body);

    functionRegistry()[lname] = def;
    return { def.display, ResultType::func, def.display };
}

CalcResult MathEngine::tryFunctionCall(const QString& expr) {
    QString e = expr.trimmed();
    if (e.contains('=')) return {};   // definitions/equations aren't handled here

    // Only proceed if the expression actually references a user function.
    // Cheap gate: find any identifier( ... ) whose name is in the registry.
    static const QRegularExpression idParen(R"(([A-Za-z][A-Za-z0-9_]*)\s*\()");
    bool referencesUserFn = false;
    {
        auto it = idParen.globalMatch(e);
        while (it.hasNext()) {
            if (isFunctionDefined(it.next().captured(1).toLower())) {
                referencesUserFn = true;
                break;
            }
        }
    }
    if (!referencesUserFn) return {};  // no user calls — let other handlers run

    // Expand every user-function call (handles embedding like 2*f(3)+1,
    // multiple calls like f(3)+g(2), and composition like f(g(x))).
    QString expanded = expandUserCalls(e);

    // If expansion left an unresolved user call (e.g. arity mismatch), the
    // evaluation below will fail and we fall through cleanly.

    // Resolve any stored variables that appear in the arguments (e.g. f(y) with
    // y = 3), then evaluate at full BigDec precision like the arithmetic path —
    // no more double rounding or 1e308 ceiling for function results.
    const QString resolved = substituteVariables(expanded);
    bool argOk = false;
    const BigDec v = BigNum::bigEvalValue(resolved, argOk);
    if (argOk) {
        const QString collapsed = BigNum::fmt(v);
        const QString full = BigNum::fmtFull(v);
        return { collapsed, ResultType::func, (full != collapsed ? full : QString()) };
    }

    // Otherwise it's symbolic (e.g. f(x+1) with x still free): simplify.
    QString simplified = Simplifier::simplify(resolved);
    if (simplified.isEmpty()) simplified = resolved;
    return { simplified, ResultType::func };
}

// -- Public accessors for user-defined functions ----------------------------
QStringList MathEngine::definedFunctionNames() {
    QStringList names;
    for (const FunctionDef& d : functionRegistry())
        names << d.name;
    return names;
}
bool MathEngine::isFunctionDefined(const QString& name) {
    return functionRegistry().contains(name.toLower());
}
const FunctionDef* MathEngine::functionDef(const QString& name) {
    auto it = functionRegistry().find(name.toLower());
    return it == functionRegistry().end() ? nullptr : &it.value();
}
void MathEngine::clearFunctions() {
    functionRegistry().clear();
}

// ----------------------------------------------------------------------
//  Persistent variables:  x = 5   then reuse x   (and `ans` = last result)
// ----------------------------------------------------------------------
static QMap<QString, QString>& variableRegistry() {
    static QMap<QString, QString> reg{ { QStringLiteral("ans"), QStringLiteral("0") } };
    return reg;   // ans starts at 0, like a physical calculator
}

// Replace every *identifier token* found in `m` with its value (parenthesised so
// precedence is safe). Token-based so "2x" substitutes the x, while "max" and
// "xy" are left alone (they aren't the variable "x").
static QString substituteMap(const QString& expr, const QMap<QString, QString>& m) {
    if (m.isEmpty()) return expr;
    static const QRegularExpression idRe(R"([A-Za-z_][A-Za-z0-9_]*)");
    QString out;
    int last = 0;
    auto it = idRe.globalMatch(expr);
    while (it.hasNext()) {
        auto mm = it.next();
        out += expr.mid(last, mm.capturedStart() - last);
        const QString tok = mm.captured(0);
        auto v = m.constFind(tok.toLower());
        out += (v != m.constEnd()) ? ("(" + v.value() + ")") : tok;
        last = mm.capturedEnd();
    }
    out += expr.mid(last);
    return out;
}

// Substitute the persistent variable registry.
static QString substituteVariables(const QString& expr) {
    return substituteMap(expr, variableRegistry());
}

QStringList MathEngine::definedVariableNames() { return variableRegistry().keys(); }
bool MathEngine::isVariableDefined(const QString& name) {
    return variableRegistry().contains(name.toLower());
}
QString MathEngine::variableValue(const QString& name) {
    return variableRegistry().value(name.toLower());
}
void MathEngine::clearVariables() {
    variableRegistry().clear();
    variableRegistry()[QStringLiteral("ans")] = QStringLiteral("0");
}

// name = value   — stores a scalar variable. Only fires when the left side is a
// bare identifier that doesn't shadow a constant/function/shape, and the right
// side evaluates to a number (using variables/functions defined so far). Anything
// else (x^2 = 5, 2x = 10, x = y with y undefined) is declined so the algebra
// solver still gets its turn.
CalcResult MathEngine::tryAssignment(const QString& expr) {
    static const QRegularExpression re(
        R"(^\s*([A-Za-z][A-Za-z0-9_]*)\s*=\s*(.+?)\s*$)");
    auto m = re.match(expr);
    if (!m.hasMatch()) return {};

    const QString name = m.captured(1);
    const QString rhs = m.captured(2).trimmed();
    const QString lname = name.toLower();

    // Never let an assignment shadow a builtin, a defined function, or a shape.
    if (builtinConstantNames().contains(lname) || builtinFunctionNames().contains(lname))
        return {};
    if (isFunctionDefined(lname)) return {};
    for (const Shape& shape : ALL_SHAPES)
        if (lname == shape.canonical || shape.aliases.contains(lname)) return {};
    if (rhs.isEmpty() || rhs.contains('=')) return {};   // chained '=' / equation

    // Evaluate the RHS with existing variables + functions resolved.
    const QString rhsResolved = expandUserCalls(substituteVariables(rhs));
    bool ok = false;
    const BigDec v = BigNum::bigEvalValue(rhsResolved, ok);
    if (!ok) return {};                                  // RHS isn't a plain value

    variableRegistry()[lname] = BigNum::fmtFull(v);      // store full precision
    variableRegistry()[QStringLiteral("ans")] = BigNum::fmtFull(v);  // last value
    const QString disp = QString("%1 = %2").arg(name, BigNum::fmt(v));
    const QString full = QString("%1 = %2").arg(name, BigNum::fmtFull(v));
    return { disp, ResultType::ok, (full != disp ? full : QString()) };
}

// -- Statement / conditional helpers ----------------------------------------

// Position of the first whole-word `word` at parenthesis depth 0; -1 if none.
static int topLevelKeyword(const QString& s, const QString& word) {
    int depth = 0;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (c == '(') { ++depth; continue; }
        if (c == ')') { if (depth > 0) --depth; continue; }
        if (depth != 0) continue;
        if (i + word.size() <= s.size() &&
            s.mid(i, word.size()).compare(word, Qt::CaseInsensitive) == 0) {
            const bool leftOk = (i == 0) || !s[i - 1].isLetterOrNumber();
            const int  end = i + word.size();
            const bool rightOk = (end >= s.size()) || !s[end].isLetterOrNumber();
            if (leftOk && rightOk) return i;
        }
    }
    return -1;
}

static bool startsWithWord(const QString& s, const QString& word) {
    const QString t = s.trimmed();
    if (!t.startsWith(word, Qt::CaseInsensitive)) return false;
    return t.size() == word.size() || !t[word.size()].isLetterOrNumber();
}

// Split on top-level occurrences of any of the given separator characters.
static QStringList splitTopLevelChars(const QString& s, const QString& seps) {
    QStringList out;
    int depth = 0, start = 0;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (c == '(') ++depth;
        else if (c == ')') { if (depth > 0) --depth; }
        else if (depth == 0 && seps.contains(c)) { out << s.mid(start, i - start); start = i + 1; }
    }
    out << s.mid(start);
    return out;
}

// Split on top-level occurrences of a whole word (e.g. "then").
static QStringList splitTopLevelWord(const QString& s, const QString& word) {
    QStringList out;
    QString rest = s;
    for (;;) {
        const int pos = topLevelKeyword(rest, word);
        if (pos < 0) { out << rest.trimmed(); break; }
        out << rest.left(pos).trimmed();
        rest = rest.mid(pos + word.size());
    }
    return out;
}

// Evaluate a boolean condition: "x = 5", "y > 3", "a != b", or a bare truthy
// value "x" (nonzero == true). Sets ok=false if a side can't be evaluated.
static bool evalCondition(const QString& cond, bool& ok) {
    ok = false;
    int depth = 0, opPos = -1;
    QString op;
    for (int i = 0; i < cond.size(); ++i) {
        const QChar c = cond[i];
        if (c == '(') { ++depth; continue; }
        if (c == ')') { if (depth > 0) --depth; continue; }
        if (depth != 0) continue;
        if (i + 1 < cond.size()) {
            const QString two = cond.mid(i, 2);
            if (two == "<=" || two == ">=" || two == "!=" || two == "==" || two == "<>") {
                op = two; opPos = i; break;
            }
        }
        if (c == '<' || c == '>' || c == '=') { op = QString(c); opPos = i; break; }
    }

    auto value = [](const QString& side, bool& sok) {
        return BigNum::bigEvalValue(expandUserCalls(substituteVariables(side.trimmed())), sok);
        };

    if (opPos < 0) {                      // no comparator -> truthy test
        bool vok = false;
        const BigDec v = value(cond, vok);
        if (!vok) return false;
        ok = true;
        return v != 0;
    }

    bool lok = false, rok = false;
    const BigDec a = value(cond.left(opPos), lok);
    const BigDec b = value(cond.mid(opPos + op.size()), rok);
    if (!lok || !rok) return false;
    ok = true;
    const BigDec eps("1e-30");
    if (op == "=" || op == "==")  return BigNum::abs(a - b) < eps;
    if (op == "!=" || op == "<>") return BigNum::abs(a - b) >= eps;
    if (op == "<")  return a < b;
    if (op == ">")  return a > b;
    if (op == "<=") return a <= b;
    if (op == ">=") return a >= b;
    ok = false;
    return false;
}

// A sequence of statements separated by ';', newline, or "then", e.g.
//   "x = 5; y = 10; x + y"   or   "x = 5 then x^2"   -> runs each in order and
// shows the last. Assignments persist between statements. A pure system of
// equations ("x+y=5; x-y=1") is left to trySystem. "then" is only a separator
// for statements that DON'T start with "if" — an "if ... then ..." conditional
// owns its "then".
CalcResult MathEngine::tryStatements(const QString& expr) {
    QStringList parts;
    for (const QString& raw : splitTopLevelChars(expr, QStringLiteral(";\n"))) {
        const QString p = raw.trimmed();
        if (p.isEmpty()) continue;
        if (startsWithWord(p, QStringLiteral("if"))) { parts << p; continue; }
        for (const QString& t : splitTopLevelWord(p, QStringLiteral("then")))
            if (!t.isEmpty()) parts << t;
    }
    if (parts.size() < 2) return {};              // not a sequence

    // A pure system of equations is solved together by trySystem, not run in
    // sequence. Conditionals and plain assignments/expressions make it a sequence.
    static const QRegularExpression assignRe(R"(^\s*[A-Za-z][A-Za-z0-9_]*\s*=\s*[^=].*$)");
    bool everyPartIsEquation = true;
    for (const QString& p : parts) {
        if (startsWithWord(p, QStringLiteral("if")) || !p.contains('=') || assignRe.match(p).hasMatch()) {
            everyPartIsEquation = false;
            break;
        }
    }
    if (everyPartIsEquation) return {};

    CalcResult last;
    for (const QString& p : parts) {
        last = evaluate(p);
        if (last.type == ResultType::err) return last;   // stop at the first error
    }
    return last;
}

// if <condition> then <result> [else <result>]  — a natural-language conditional.
// The condition is a comparison (=, ==, !=, <, >, <=, >=) or a truthy value.
CalcResult MathEngine::tryConditional(const QString& expr) {
    const QString e = expr.trimmed();
    if (!startsWithWord(e, QStringLiteral("if"))) return {};

    const QString rest = e.mid(2).trimmed();                 // drop leading "if"
    const int thenPos = topLevelKeyword(rest, QStringLiteral("then"));
    if (thenPos < 0)
        return { "Expected 'then' after 'if <condition>'", ResultType::err };

    const QString cond = rest.left(thenPos).trimmed();
    const QString after = rest.mid(thenPos + 4).trimmed();   // "then" is 4 chars

    QString thenBranch = after, elseBranch;
    const int elsePos = topLevelKeyword(after, QStringLiteral("else"));
    if (elsePos >= 0) {
        thenBranch = after.left(elsePos).trimmed();
        elseBranch = after.mid(elsePos + 4).trimmed();       // "else" is 4 chars
    }
    if (cond.isEmpty() || thenBranch.isEmpty())
        return { "Malformed if — need 'if <condition> then <result>'", ResultType::err };

    bool ok = false;
    const bool truth = evalCondition(cond, ok);
    if (!ok) return { "Could not evaluate the condition", ResultType::err };

    if (truth)                 return evaluate(thenBranch);
    if (!elseBranch.isEmpty()) return evaluate(elseBranch);
    return { "Condition not met", ResultType::ok };
}

// EXPR where <bindings>  — evaluate EXPR with LOCAL variable bindings that do NOT
// persist, e.g. "find x^x where x = 5" -> 3125. Bindings are separated by ',' or
// "and": "a^2 + b^2 where a = 3, b = 4" -> 25. ("find" is already stripped by the
// natural-language preprocessor.)
CalcResult MathEngine::tryWhere(const QString& expr) {
    const int wherePos = topLevelKeyword(expr, QStringLiteral("where"));
    if (wherePos < 0) return {};

    const QString body = expr.left(wherePos).trimmed();
    const QString bindingsStr = expr.mid(wherePos + 5).trimmed();   // "where" is 5
    if (body.isEmpty() || bindingsStr.isEmpty()) return {};

    // Collect "name = value" bindings, split on ',' and "and".
    QMap<QString, QString> local;
    static const QRegularExpression bindRe(R"(^([A-Za-z][A-Za-z0-9_]*)\s*=\s*(.+)$)");
    for (const QString& piece : splitTopLevelChars(bindingsStr, QStringLiteral(","))) {
        for (const QString& raw : splitTopLevelWord(piece, QStringLiteral("and"))) {
            const QString b = raw.trimmed();
            if (b.isEmpty()) continue;
            const auto m = bindRe.match(b);
            if (!m.hasMatch()) return {};                 // not a clean binding → decline
            // A binding value may use earlier bindings and persistent variables.
            bool vok = false;
            const BigDec v = BigNum::bigEvalValue(
                expandUserCalls(substituteVariables(substituteMap(m.captured(2).trimmed(), local))), vok);
            if (!vok) return {};
            local[m.captured(1).toLower()] = BigNum::fmtFull(v);
        }
    }
    if (local.isEmpty()) return {};

    // Substitute the locals into the body and evaluate — locals win over any
    // persistent variable of the same name, and never leak into the registry.
    return evaluate(substituteMap(body, local));
}

static QString preprocessNaturalLanguage(const QString& expr) {
    QString e = expr.toLower();
    e = NaturalLanguage::preprocess(e);
    /*

    // ------------------------------------------------------------------
    // TABLE OF SIMPLE WORD REPLACEMENTS (fractions, numbers, etc.)
    // Add/remove lines here as needed – no other code changes.
    // Format: { "phrase", "replacement" }
    // ------------------------------------------------------------------
    static const struct { const char* phrase; const char* replacement; } wordMap[] = {
        // Fractions
        {"half",       "1/2"},
        {"a half",     "1/2"},
        {"one half",   "1/2"},
        {"quarter",    "1/4"},
        {"a quarter",  "1/4"},
        {"one quarter","1/4"},
        {"third",      "1/3"},
        {"one third",  "1/3"},
        {"two thirds", "2/3"},
        {"three quarters", "3/4"},
        {"eighth",     "1/8"},
        {"an eighth",  "1/8"},
        {"dozen",     "12"},
        {"score",     "20"},
    };

    for (const auto& w : wordMap) {
        e.replace(w.phrase, w.replacement);
    }



   // ------------------------------------------------------------------
    // BINARY OPERATOR WORDS: "X word Y" → "X op Y" (with possible reorder)
    // ------------------------------------------------------------------
    // "of"  → multiplication (X * Y)
    static QRegularExpression ofRe(R"(([^\s]+)\s+of\s+([^\s]+))");
    e.replace(ofRe, "(\\1 * \\2)");

    // "into" → division with swapped operands: X into Y  → (Y / X)
    static QRegularExpression intoRe(R"(([^\s]+)\s+into\s+([^\s]+))");
    e.replace(intoRe, "(\\2 * \\1)");

    // "times" → multiplication (same as "of")
    static QRegularExpression timesRe(R"(([^\s]+)\s+times\s+([^\s]+))");
    e.replace(timesRe, "(\\1 * \\2)");


    static QRegularExpression byRe(R"(([^\s]+)\s+by\s+([^\s]+))");
    e.replace(byRe, "(\\1 / \\2");
    */
    return e;
}
// A genuine math/domain error (report it) vs a parse failure (decline, so other
// handlers still get a turn). Parse failures name a token/structure problem.
static bool isDomainError(const QString& msg) {
    if (msg.isEmpty()) return false;
    static const char* parseHints[] = { "identifier", "unexpected", "missing", "expected", "char" };
    const QString lower = msg.toLower();
    for (const char* h : parseHints)
        if (lower.contains(QLatin1String(h))) return false;
    return true;
}

// ----------------------------------------------------------------------
//  tryArithmetic – fallback for numeric expressions
// ----------------------------------------------------------------------
CalcResult MathEngine::tryArithmetic(const QString& expr) {
    // expr has already been through NaturalLanguage::preprocess() in
    // evaluate() (its only caller) — re-running the full regex pipeline here
    // on an already-normalised string was pure wasted work on every keystroke.
    const QString& processed = expr;

    // -- Common measurement conversions (natural language) --------------------
// Handles: "1 furlong", "3 furlongs in meters", "2 nautical miles"
// These are fixed-unit lookups — no "to" keyword needed



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
        { {"kilometers", "km", "kilometres", "kilometer", "kilometre"}, 1000, "meters"},
        {{"feet", "ft"}, 0.3048, "meters"},

    };


    // Integer exponentiation: a^b where both a and b are integers
    static QRegularExpression intPowRe(R"(^\s*(\d+)\s*\^\s*(\d+)\s*$)");
    auto match = intPowRe.match(processed);
    if (match.hasMatch()) {
        BigInt base(match.captured(1).toStdString());
        BigInt exp(match.captured(2).toStdString());
        try {

            QString result = BigNum::bigPow(base, exp);
            return { result, ResultType::big };
        }
        catch (const std::exception& e) {
            return { QString("Error: ") + e.what(), ResultType::err };
        }
    }

    static QRegularExpression measRe(
        R"(^([\d.]+)\s+(.+)$)",
        QRegularExpression::CaseInsensitiveOption);
    auto mm = measRe.match(processed.trimmed());
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
                        ResultType::conv
                    };
                }
            }
        }
    }
    // General arithmetic via BigDec — arbitrary precision, so results are no
    // longer capped at the double overflow point (~1.8e308). The collapsed form
    // (~15 sig figs, auto-scientific) is shown; the full-digit form rides along
    // in the formula field so the UI can expand/collapse it. bigEvalValue sets
    // ok=false for anything it can't parse, and we fall back to the double
    // evaluator for the rare constructs it doesn't cover.
    QString berr;
    {
        bool bok = false;
        const BigDec bv = BigNum::bigEvalValue(processed, bok, &berr);
        if (bok) {
            const QString collapsed = BigNum::fmt(bv);
            const QString full = BigNum::fmtFull(bv);
            const QString expandForm = (full != collapsed) ? full : QString();
            return { collapsed, ResultType::ok, expandForm };
        }
    }

    // Fallback to the double evaluator (covers anything BigDec declined).
    bool ok;
    double v = evalSimple(processed, ok);
    if (ok) return { fmt(v), ResultType::ok };

    // Both failed. If BigDec hit a genuine math/domain error (sqrt of negative,
    // division by zero, log of a non-positive, ...) rather than a plain parse
    // failure, surface it — far clearer than a blanket "cannot parse".
    if (isDomainError(berr)) return { berr, ResultType::err };
    return {};
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
static bool isValidIdentifier(const QString& name, const VarMap& vars) {
    if (builtinConstantNames().contains(name) || builtinFunctionNames().contains(name)) return true;
    if (vars.contains(name)) return true;
    return false;
}

// ----------------------------------------------------------------------
//  Main dispatcher
// ----------------------------------------------------------------------
CalcResult MathEngine::evaluate(const QString& expr) {
    CalcResult r = dispatchEvaluate(expr);
    // Record the last numeric answer as `ans` — only when the result is a bare
    // number we can reuse (skips assignment confirmations, equality proofs,
    // errors, and geometry cards).
    if (r.type == ResultType::ok || r.type == ResultType::big || r.type == ResultType::trig) {
        const QString candidate = r.formula.isEmpty() ? r.result : r.formula;
        bool numeric = false;
        BigNum::bigEvalValue(candidate, numeric);
        if (numeric) variableRegistry()["ans"] = candidate;
    }
    return r;
}

CalcResult MathEngine::dispatchEvaluate(const QString& expr) {
    if (expr.trimmed().isEmpty()) throw std::runtime_error("Empty expression");

    // Preprocess FIRST, so statement splitting and every downstream classifier
    // work on the same normalised text. Otherwise a natural-language system like
    // "x plus y equals 5 then x minus y equals 1" would be split before its
    // "equals" became "=", and mis-handled as a sequence instead of a system.
    QString processed = NaturalLanguage::preprocess(expr);

    // Conditional ("if x = 5 then ... else ...") — checked before statements so
    // its "then" is the conditional keyword, not a statement separator.
    { CalcResult r = tryConditional(processed); if (r.type != ResultType::none) return r; }

    // Multi-statement sequence ("x = 5; y = 10; x + y" or "... then ...") — run
    // each statement in order (a pure system of equations is left to trySystem).
    { CalcResult r = tryStatements(processed); if (r.type != ResultType::none) return r; }

    // "EXPR where x = 5" — evaluate EXPR with local (non-persistent) bindings.
    { CalcResult r = tryWhere(processed); if (r.type != ResultType::none) return r; }

    // "vars" / "variables" — list what's currently stored.
    if (processed == "vars" || processed == "variables") {
        const QStringList names = variableRegistry().keys();
        if (names.isEmpty()) return { "No variables defined", ResultType::ok };
        QStringList lines;
        for (const QString& n : names) {
            // Values are stored at full precision; show the collapsed form.
            bool vok = false;
            const BigDec v = BigNum::bigEvalValue(variableRegistry().value(n), vok);
            lines << QString("%1 = %2").arg(n, vok ? BigNum::fmt(v) : variableRegistry().value(n));
        }
        return { lines.join("\n"), ResultType::ok };
    }

    // "clear vars" / "reset vars" — wipe all stored variables.
    if (processed == "clear vars" || processed == "reset vars" || processed == "clearvars") {
        variableRegistry().clear();
        return { "Variables cleared", ResultType::ok };
    }

    // Factorial
    static QRegularExpression factRe(
        R"(^\s*(?:fact\s*\(\s*(\d+)\s*\)|(\d+)\s*!)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto match = factRe.match(processed);
    if (match.hasMatch()) {
        QString nStr = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        BigInt n(nStr.toStdString());
        if (n < 0) return { "Negative factorial not allowed", ResultType::err };
        try {
            return { BigNum::bigFactorial(n), ResultType::big };
        }
        catch (const std::exception& e) {
            return { QString("Error: ") + e.what(), ResultType::err };
        }
    }

    CalcResult r;

    // User-defined functions: "f(x) = x + 5" (definition) or "f(3)" (call).
    // Checked first so a defined function name can never be mis-parsed as a
    // unit conversion, shape command, or plain arithmetic below.
    r = tryFunctionDefinition(processed); if (r.type != ResultType::none) return r;
    r = tryFunctionCall(processed);       if (r.type != ResultType::none) return r;

    // Variable assignment ("x = 5") — before algebra, so it stores instead of
    // being "solved". Then substitute every stored variable into the rest of the
    // expression so the remaining handlers see plain numbers.
    r = tryAssignment(processed);         if (r.type != ResultType::none) return r;
    processed = substituteVariables(processed);

    // Explicit "factor(...)" keyword — checked before conversion/geometry so
    // "factor" is never mistaken for a unit or shape.
    r = tryFactor(processed);      if (r.type != ResultType::none) return r;

    r = tryConversion(processed);  if (r.type != ResultType::none) return r;
    r = tryGeometry(processed);    if (r.type != ResultType::none) return r;
    r = tryTrig(processed);        if (r.type != ResultType::none) return r;

    if (!processed.contains('=') && !Expr::detectVariables(processed).isEmpty()) {
        const QString simplified = Simplifier::simplify(processed);
        // Only treat it as a simplification if terms actually combined — compare
        // ignoring whitespace, so "y + 3" -> "y+3" isn't reported as a "result"
        // (that expression is really an undefined variable, handled below).
        if (!simplified.isEmpty()
            && QString(simplified).remove(' ') != QString(processed).remove(' '))
            return { simplified, ResultType::alg };
    }

    // If an equation references user-defined functions (e.g. "f(x) + 3 = 10"
    // with f defined earlier), expand those calls first, then let the algebra
    // path solve the resulting plain equation. We only do this when the input
    // wasn't already consumed as a definition/call above, and when it actually
    // contains a user-function reference — otherwise nothing changes.
    if (processed.contains('=')) {
        static const QRegularExpression idParen(R"(([A-Za-z][A-Za-z0-9_]*)\s*\()");
        bool refsUserFn = false;
        auto it = idParen.globalMatch(processed);
        while (it.hasNext()) {
            if (isFunctionDefined(it.next().captured(1).toLower())) { refsUserFn = true; break; }
        }
        if (refsUserFn) {
            QString expanded = expandUserCalls(processed);
            if (expanded != processed) {
                // Re-run the algebra/system handlers on the expanded equation.
                r = trySystem(expanded);   if (r.type != ResultType::none) return r;
                r = tryAlgebra(expanded);  if (r.type != ResultType::none) return r;
            }
        }
    }

    // Systems of equations (multiple '=') must be tried before tryAlgebra,
    // which only accepts a single '='.
    r = trySystem(processed);      if (r.type != ResultType::none) return r;

    r = tryAlgebra(processed);     if (r.type != ResultType::none) return r;
    r = tryArithmetic(processed);  if (r.type != ResultType::none) return r;

    // Nothing handled it. If the leftover expression still references an
    // identifier we don't know (stored variables were already substituted
    // above), name it — much friendlier than a blanket "cannot parse".
    const QSet<QString> unknown = Expr::detectVariables(processed);
    if (!unknown.isEmpty()) {
        QStringList names(unknown.begin(), unknown.end());
        names.sort();
        throw std::runtime_error(
            (names.size() == 1 ? "'" + names.first() + "' is not defined — assign it first, e.g. "
                + names.first() + " = 5"
                : "Undefined: " + names.join(", ")).toStdString());
    }
    throw std::runtime_error("Cannot parse expression");
}