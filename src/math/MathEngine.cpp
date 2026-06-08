#include "MathEngine.h"
#include "BigNum.h"
#include "Expr.h"
#include "Solver.h"
#include <thread/PersistentWorker.h>
#include "Algebra.h"
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <stdexcept>
#include <algorithm>
#include <set>
#include "shapes/ShapeDef.h"
#include "NaturalLanguage.h"
#include "Simplifier.h"


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
            // Use the static cancel flag from PersistentWorker
            extern std::atomic<bool> g_cancelFlag; // we'll make it accessible
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

    QString solution = Algebra::solveEquation(lhs, rhs);
    if (solution.isEmpty()) return {};
    return { solution, "alg" };
}

static QString preprocessNaturalLanguage(const QString& expr) {
   QString e = expr.toLower();
   e = NaturalLanguage::preprocess(expr);
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
// ----------------------------------------------------------------------
//  tryArithmetic – fallback for numeric expressions
// ----------------------------------------------------------------------
CalcResult MathEngine::tryArithmetic(const QString& expr) {
QString processed = NaturalLanguage::preprocess(expr);

    // ── Common measurement conversions (natural language) ────────────────────
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
            { {"kilometers", "km", "kilometres"}, 1000, "meters"},
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
                return { result, "big" };
            }
            catch (const std::exception& e) {
                return { QString("Error: ") + e.what(), "err" };
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
                            "conv"
                        };
                    }
                }
            }
        }
    

        bool bok;
        QString bigResult = bigEval(processed, bok);
        if (bok) {
            // bigEval returns a decimal; but we want exact integer if result is integer.
            // For now, return as "big". If the string contains only digits, it's exact.
            return { bigResult, "big" };
        }
        // fallback to double evaluation
        bool ok;
        double v = evalSimple(processed, ok);
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
static bool isValidIdentifier(const QString& name, const VarMap& vars) {
    // Built-in constants
    static const QSet<QString> constants = { "pi", "e", "tau", "inf" };
    // Built-in functions (must match the ones in callBuiltin)
    static const QSet<QString> functions = {
        "sin","cos","tan","sinh","cosh","tanh","asin","acos","atan",
        "asinh","acosh","atanh","atan2","sinr","cosr","tanr","asinr","acosr","atanr",
        "sqrt","cbrt","abs","log","ln","log2","exp","floor","ceil","round","sign",
        "min","max","pow","mod","hypot","fact","gcd","hcf","lcm","ncr","npr","c","logbase"
    };
    if (constants.contains(name) || functions.contains(name)) return true;
    if (vars.contains(name)) return true;
    return false;
}

// ----------------------------------------------------------------------
//  Main dispatcher
// ----------------------------------------------------------------------
CalcResult MathEngine::evaluate(const QString& expr) {
    if (expr.trimmed().isEmpty()) throw std::runtime_error("Empty expression");

    // Detect standalone factorial expressions: "fact(50)" or "50!" 
    static QRegularExpression factRe(
        R"(^\s*(?:fact\s*\(\s*(\d+)\s*\)|(\d+)\s*!)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto match = factRe.match(expr);
    if (match.hasMatch()) {
        QString nStr = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        BigInt n(nStr.toStdString());
        if (n < 0) return { "Negative factorial not allowed", "err" };
        try {
            QString result = BigNum::bigFactorial(n);
            return { result, "big" };
        }
        catch (const std::exception& e) {
            return { QString("Error: ") + e.what(), "err" };
        }
    }


    if (expr.trimmed().isEmpty()) throw std::runtime_error("Empty expression");
    CalcResult r;
    r = tryConversion(expr);  if (!r.type.isEmpty()) return r;
    r = tryGeometry(expr);    if (!r.type.isEmpty()) return r;
    r = tryTrig(expr);        if (!r.type.isEmpty()) return r;

    // If expression has variables but no '=', try to simplify it
    if (!expr.contains('=') && !Expr::detectVariables(expr).isEmpty()) {
        QString simplified = Simplifier::simplify(expr);
        if (!simplified.isEmpty() && simplified != expr) {
            return { simplified, "alg" };
        }
    }

    r = tryAlgebra(expr);     if (!r.type.isEmpty()) return r;
    r = tryArithmetic(expr);  if (!r.type.isEmpty()) return r;
    throw std::runtime_error("Cannot parse expression");
}