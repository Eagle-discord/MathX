#include "BigNum.h"
#include <thread/PersistentWorker.h>
#include <stdexcept>
#include <functional>
#include <cmath>
#include <QDebug>

// -- bigPow --------------------------------------------------------------------
QString BigNum::bigPow(BigInt base, BigInt exp,
    std::function<void(int)> progressCallback,
    const std::atomic<bool>* cancelFlag)
{
    if (exp < 0) throw std::runtime_error("Negative exponent not supported");

    // Same uninterruptible-str() trap as bigFactorial, and worse here: this is
    // binary exponentiation, so 2^100000000 finishes the LOOP in ~27 steps --
    // the progress bar flies to 100% -- and then sits for minutes inside
    // result.str() formatting ~30 million digits, with Stop doing nothing.
    //
    // Unlike a factorial, the size is knowable up front: digits(base^exp) is
    // exp * log10(base). Refuse before doing any work rather than after.
    // The exp > 1e9 gate first: casting a huge cpp_int to double gives inf, and
    // inf * anything is inf, which would still trip the check -- but relying on
    // that is the kind of thing that breaks when someone changes the backend.
  

    BigInt result = 1;
    BigInt currentBase = base;
    BigInt remainingExp = exp;

    int steps = 0;
    for (BigInt tmp = exp; tmp > 0; tmp >>= 1) ++steps;

    int step = 0;
    while (remainingExp > 0) {
        if (cancelFlag && cancelFlag->load())
            throw std::runtime_error("Cancelled");

        if (remainingExp % 2 == 1)
            result *= currentBase;

        currentBase *= currentBase;
        remainingExp >>= 1;

        if (progressCallback)
            progressCallback((++step * 100) / steps);
    }

    return QString::fromStdString(result.str());
}

// -- bigFactorial --------------------------------------------------------------
// progressCallback reports the multiply loop 0..100. phaseCallback, if given,
// is told when the loop ends and the base-2 -> base-10 conversion begins: for
// large n that conversion is a real wait (100000! is 456574 digits) and used to
// present as a freeze at 100%.
QString BigNum::bigFactorial(BigInt n,
    std::function<void(int)> progressCallback,
    std::function<void(const QString&)> phaseCallback)
{
    if (n < 0) throw std::runtime_error("Unable to calculate a factorial of negative number");

    // Refuse up front rather than cancel later. The multiply loop below polls
    // s_cancel every iteration and stops instantly -- but the str() at the end
    // does NOT and cannot: it's one call into Boost's base-2 -> base-10
    // conversion with no callback, no hook, and no way to interrupt it. For
    // 1000000! that is ~5.5 million digits and takes minutes, during which the
    // progress bar reads 100% and Stop does nothing. That is not a cancellation
    // bug, it's an uninterruptible call, and the only fix is to not enter it.
    //
    // 100000! is ~456k digits and converts in a few seconds -- tolerable, and
    // well past anything a person types on purpose.
    if (n > 1000000)
        throw std::runtime_error(
            "Factorial too large (max 1000000)");

    BigInt result = 1;
    for (BigInt i = 2; i <= n; ++i) {
        // Throw, don't return "Cancelled". Returning it as a value meant the
        // worker emitted ResultType::big and the UI rendered a "result" whose
        // text was the word Cancelled -- copyable, expandable, indistinguishable
        // from a real answer. bigPow and factorialBig both throw here; the catch
        // in PersistentWorker::process already turns it into a proper err result.
        if (PersistentWorker::s_cancel.load())
            throw std::runtime_error("Cancelled");
        result *= i;
        if (progressCallback && i % 1000 == 0)
            progressCallback(static_cast<int>((i * 100) / n));
    }
    if (phaseCallback) phaseCallback(QStringLiteral("Formatting"));
    // Not cancellable: str() is a single call into Boost with no hook. It is the
    // one unbounded step left in this function.
    return QString::fromStdString(result.str());
}

// -- Expression parser ---------------------------------------------------------
struct BTok {
    const QString& s;
    int pos = 0;

    explicit BTok(const QString& str) : s(str) {}

    void skip() {
        while (pos < s.size() && s[pos].isSpace()) ++pos;
    }
    QChar peek() { skip(); return pos < s.size() ? s[pos] : QChar(0); }
    QChar get() { skip(); return pos < s.size() ? s[pos++] : QChar(0); }
    bool atEnd() { skip(); return pos >= s.size(); }

    // Returns a QStringView slice — zero allocation
    QStringView readDigits() {
        skip();
        int start = pos;
        while (pos < s.size() && s[pos].isDigit()) ++pos;
        return QStringView(s).mid(start, pos - start);
    }
    QString readWord() {
        skip();
        int start = pos;
        while (pos < s.size() && (s[pos].isLetterOrNumber() || s[pos] == '_')) ++pos;
        return s.mid(start, pos - start);
    }
};

static BigDec bExpr(BTok&);
static BigDec bTerm(BTok&);
static BigDec bUnary(BTok&);
static BigDec bPow(BTok&);
static BigDec bPostfix(BTok&);
static BigDec bPrim(BTok&);

// Exact factorial of a non-negative integer BigDec, via BigInt. Shared by the
// fact() function and the postfix '!' operator. Bounded so a stray "50000!"
// inside an expression can't hang the evaluator (standalone n! goes through the
// worker's bigFactorial path with progress instead).
static BigDec factorialBig(const BigDec& x)
{
    namespace bmp = boost::multiprecision;
    if (x < 0 || x != bmp::floor(x))
        throw std::runtime_error("Factorial needs a non-negative integer");
    BigInt n(x.str(0, std::ios_base::fixed));

    // The 10000 cap is a guess at "how long will the user tolerate". It exists
    // because this loop, unlike BigNum::bigFactorial, had no cancellation --
    // fact(10000) inside an expression pinned the worker with no way out and no
    // progress bar. Now that we honour s_cancel the cap can be generous: the
    // Stop button works, so the only reason for a limit is to catch obvious
    // nonsense like fact(1e9) before it allocates for an hour.
    if (n > 1000000)
        throw std::runtime_error("Factorial too large inside expression (max 1000000)");

    BigInt r = 1;
    int sinceCheck = 0;
    for (BigInt i = 2; i <= n; ++i) {
        // Checked every 256 iterations rather than every one: an atomic load per
        // multiply is measurable on small factorials, which are the common case.
        // 256 iterations of even a large multiply is well under a frame. The
        // counter is a plain int, not BigInt arithmetic -- masking a cpp_int
        // every pass would cost more than the check it guards.
        if (++sinceCheck >= 1000) {
            sinceCheck = 0;
            if (PersistentWorker::s_cancel.load())
                throw std::runtime_error("Cancelled");
        }
        r *= i;
    }
    return BigDec(r.str());
}

static BigDec bCallFn(const QString& name, BTok& t)
{
    if (t.peek() != '(') throw std::runtime_error("Expected '('");
    t.get();
    QVector<BigDec> args;
    if (t.peek() != ')') {
        args.append(bExpr(t));
        while (t.peek() == ',') { t.get(); args.append(bExpr(t)); }
    }
    t.skip();
    if (t.peek() != ')') throw std::runtime_error("Missing ')'");
    t.get();

    namespace bmp = boost::multiprecision;
    const std::string fn = name.toStdString();
    auto need = [&](int n) {
        if (args.size() < n)
            throw std::runtime_error(fn + " needs " + std::to_string(n) + " argument(s)");
        };
    const BigDec DEG = BigNum::PI / 180;         // degrees -> radians
    const BigDec RAD = BigDec(180) / BigNum::PI; // radians -> degrees

    // Trigonometry — degree-based by default, radian variants suffixed 'r'
    // (matches the double evaluator in Expr.cpp).
    if (fn == "sin") { need(1); return bmp::sin(args[0] * DEG); }
    if (fn == "cos") { need(1); return bmp::cos(args[0] * DEG); }
    if (fn == "tan") { need(1); return bmp::tan(args[0] * DEG); }
    if (fn == "asin") { need(1); return bmp::asin(args[0]) * RAD; }
    if (fn == "acos") { need(1); return bmp::acos(args[0]) * RAD; }
    if (fn == "atan") { need(1); return bmp::atan(args[0]) * RAD; }
    if (fn == "sinr") { need(1); return bmp::sin(args[0]); }
    if (fn == "cosr") { need(1); return bmp::cos(args[0]); }
    if (fn == "tanr") { need(1); return bmp::tan(args[0]); }
    if (fn == "asinr") { need(1); return bmp::asin(args[0]); }
    if (fn == "acosr") { need(1); return bmp::acos(args[0]); }
    if (fn == "atanr") { need(1); return bmp::atan(args[0]); }
    if (fn == "atan2") { need(2); return bmp::atan2(args[0], args[1]) * RAD; }

    // Hyperbolic
    if (fn == "sinh") { need(1); return bmp::sinh(args[0]); }
    if (fn == "cosh") { need(1); return bmp::cosh(args[0]); }
    if (fn == "tanh") { need(1); return bmp::tanh(args[0]); }
    if (fn == "asinh") { need(1); return bmp::asinh(args[0]) * RAD; }
    if (fn == "acosh") { need(1); return bmp::acosh(args[0]) * RAD; }
    if (fn == "atanh") { need(1); return bmp::atanh(args[0]) * RAD; }

    // Powers, roots, logarithms
    if (fn == "sqrt") { need(1); if (args[0] < 0) throw std::runtime_error("sqrt of negative"); return bmp::sqrt(args[0]); }
    if (fn == "cbrt") { need(1); return bmp::cbrt(args[0]); }
    if (fn == "abs") { need(1); return bmp::abs(args[0]); }
    if (fn == "exp") { need(1); return bmp::exp(args[0]); }
    if (fn == "ln") { need(1); if (args[0] <= 0) throw std::runtime_error("ln of non-positive"); return bmp::log(args[0]); }
    if (fn == "log") { need(1); if (args[0] <= 0) throw std::runtime_error("log of non-positive"); return bmp::log10(args[0]); }
    if (fn == "log2") { need(1); if (args[0] <= 0) throw std::runtime_error("log2 of non-positive"); return bmp::log(args[0]) / bmp::log(BigDec(2)); }
    // logbase(value, base) -- log(value)/log(base). The operands were reversed
    // here exactly as they were in Expr::callBuiltin (BUG-006): logbase(8,2)
    // returned log(2)/log(8) = 0.333 instead of 3. THIS is the copy that runs --
    // tryArithmetic reaches bigEvalValue before Expr, so fixing only the Expr
    // one changed nothing observable.
    if (fn == "logbase") { need(2); if (args[0] <= 0 || args[1] <= 0) throw std::runtime_error("logbase of non-positive"); return bmp::log(args[0]) / bmp::log(args[1]); }
    if (fn == "pow") { need(2); return bmp::pow(args[0], args[1]); }
    if (fn == "hypot") { need(2); return bmp::sqrt(args[0] * args[0] + args[1] * args[1]); }

    // Rounding / sign
    if (fn == "floor") { need(1); return bmp::floor(args[0]); }
    if (fn == "ceil") { need(1); return bmp::ceil(args[0]); }
    if (fn == "round") { need(1); return args[0] >= 0 ? bmp::floor(args[0] + BigDec("0.5")) : bmp::ceil(args[0] - BigDec("0.5")); }
    if (fn == "sign") { need(1); return BigDec(args[0] > 0 ? 1 : (args[0] < 0 ? -1 : 0)); }
    if (fn == "mod") { need(2); return bmp::fmod(args[0], args[1]); }

    // Aggregates (variadic)
    if (fn == "min") { need(1); BigDec r = args[0]; for (int i = 1; i < args.size(); ++i) if (args[i] < r) r = args[i]; return r; }
    if (fn == "max") { need(1); BigDec r = args[0]; for (int i = 1; i < args.size(); ++i) if (args[i] > r) r = args[i]; return r; }
    if (fn == "sum") { need(1); BigDec r = 0; for (const auto& a : args) r += a; return r; }
    if (fn == "prod") { need(1); BigDec r = 1; for (const auto& a : args) r *= a; return r; }
    if (fn == "avg") { need(1); BigDec r = 0; for (const auto& a : args) r += a; return r / args.size(); }

    // Integer-valued number theory (exact via BigInt)
    auto toInt = [&](const BigDec& x) -> BigInt {
        if (x != bmp::floor(x)) throw std::runtime_error(fn + " needs integer arguments");
        return BigInt(x.str(0, std::ios_base::fixed));
        };
    auto iabs = [](BigInt v) { return v < 0 ? BigInt(-v) : v; };
    if (fn == "gcd" || fn == "hcf") {
        need(2);
        BigInt g = iabs(toInt(args[0]));
        for (int i = 1; i < args.size(); ++i) g = boost::multiprecision::gcd(g, iabs(toInt(args[i])));
        return BigDec(g.str());
    }
    if (fn == "lcm") {
        need(2);
        BigInt l = iabs(toInt(args[0]));
        for (int i = 1; i < args.size(); ++i) {
            BigInt b = iabs(toInt(args[i]));
            if (l == 0 || b == 0) { l = 0; break; }
            l = l / boost::multiprecision::gcd(l, b) * b;
        }
        return BigDec(l.str());
    }
    if (fn == "ncr" || fn == "c") {
        need(2);
        BigInt n = toInt(args[0]), r = toInt(args[1]);
        if (r < 0 || r > n) return BigDec(0);
        if (r > n - r) r = n - r;
        BigInt res = 1;
        for (BigInt i = 0; i < r; ++i) { res *= (n - i); res /= (i + 1); }
        return BigDec(res.str());
    }
    if (fn == "npr") {
        need(2);
        BigInt n = toInt(args[0]), r = toInt(args[1]);
        if (r < 0 || r > n) return BigDec(0);
        BigInt res = 1;
        for (BigInt i = 0; i < r; ++i) res *= (n - i);
        return BigDec(res.str());
    }
    if (fn == "fact") { need(1); return factorialBig(args[0]); }

    throw std::runtime_error("Unknown function: " + fn);
}

static BigDec bPrim(BTok& t)
{
    t.skip();
    if (t.peek() == '(') {
        t.get();
        BigDec v = bExpr(t);
        t.skip();
        // A missing ')' used to be shrugged off, so "(2+3" quietly evaluated to
        // 5 (BUG-020). Expr's parser throws here, but tryArithmetic reaches
        // bigEvalValue FIRST -- so the lenient parser won and the strict one
        // never ran. bCallFn already throws "Missing ')'" a few lines up; this
        // just makes grouping agree with it.
        if (t.peek() != ')') throw std::runtime_error("Missing ')'");
        t.get();
        return v;
    }
    if (t.peek().isDigit() || t.peek() == '.') {
        // Build number string using index slicing — no char-by-char append
        t.skip();
        int start = t.pos;
        while (t.pos < t.s.size() && t.s[t.pos].isDigit()) ++t.pos;
        if (t.pos < t.s.size() && t.s[t.pos] == '.') {
            ++t.pos;
            while (t.pos < t.s.size() && t.s[t.pos].isDigit()) ++t.pos;
        }
        // Scientific exponent (e/E [+/-] digits). BigDec parses it natively.
        if (t.pos < t.s.size() && (t.s[t.pos] == 'e' || t.s[t.pos] == 'E')) {
            int j = t.pos + 1;
            if (j < t.s.size() && (t.s[j] == '+' || t.s[j] == '-')) ++j;
            if (j < t.s.size() && t.s[j].isDigit()) {
                t.pos = j + 1;
                while (t.pos < t.s.size() && t.s[t.pos].isDigit()) ++t.pos;
            }
        }
        // QStringView → std::string: one allocation, no char loop
        return BigDec(QStringView(t.s).mid(start, t.pos - start).toString().toStdString());
    }
    if (t.peek().isLetter()) {
        QString word = t.readWord();
        t.skip();
        if (word == "pi")  return BigNum::PI;
        if (word == "e")   return BigNum::E;
        if (word == "tau") return 2 * BigNum::PI;
        if (t.peek() == '(') return bCallFn(word.toLower(), t);
        throw std::runtime_error("Unknown bignum identifier: " + word.toStdString());
    }
    throw std::runtime_error("Unexpected char in bignum expression");
}

// Postfix operators bind tighter than '^': "2^3!" is "2^(3!)". '!' is exact
// factorial; '%' is percent (divide by 100), matching the double evaluator.
static BigDec bPostfix(BTok& t)
{
    BigDec v = bPrim(t);
    while (true) {
        t.skip();
        const QChar c = t.peek();
        if (c == '!') { t.get(); v = factorialBig(v); }
        else if (c == '%') { t.get(); v = v / 100; }
        else break;
    }
    return v;
}

static BigDec bPow(BTok& t)
{
    BigDec base = bPostfix(t);
    t.skip();
    if (t.peek() == '^') {
        t.get();
        return BigNum::pow(base, bUnary(t)); // right-associative, allows signed exponents
    }
    return base;
}

// Unary +/- binds looser than '^' (so -2^2 == -(2^2) == -4), but tighter
// than * / so it can appear right after those operators (e.g. 2*-3).
static BigDec bUnary(BTok& t)
{
    t.skip();
    if (t.peek() == '-') { t.get(); return -bUnary(t); }
    if (t.peek() == '+') { t.get(); return bUnary(t); }
    return bPow(t);
}

static BigDec bTerm(BTok& t)
{
    BigDec v = bUnary(t);
    while (true) {
        t.skip();
        QChar op = t.peek();
        if (op != '*' && op != '/') break;
        t.get();
        BigDec r = bUnary(t);
        // No BigDec(...) wrapping — let expression templates evaluate lazily
        if (op == '*') v *= r;
        else {
            if (r == 0) throw std::runtime_error("Division by zero");
            v /= r;
        }
    }
    return v;
}

static BigDec bExpr(BTok& t)
{
    BigDec v = bTerm(t);
    while (true) {
        t.skip();
        QChar op = t.peek();
        if (op != '+' && op != '-') break;
        t.get();
        BigDec r = bTerm(t);
        if (op == '+') v += r;
        else           v -= r;
    }
    return v;
}

BigDec BigNum::bigEvalValue(const QString& input, bool& ok, QString* err)
{
    try {
        // Normalise in a single pass instead of repeated replace() calls
        QString r;
        r.reserve(input.size() + 16);
        const QString s = input.simplified();

        for (int i = 0; i < s.size(); ) {
            QChar c = s[i];
            // Replace bracket variants
            if (c == '[' || c == '{') { r += '('; ++i; continue; }
            if (c == ']' || c == '}') { r += ')'; ++i; continue; }
            // Replace ** with ^
            if (c == '*' && i + 1 < s.size() && s[i + 1] == '*') {
                r += '^'; i += 2; continue;
            }
            // Consume a whole numeric literal (including a scientific exponent)
            // so "2e5" isn't split into "2*e5" by the implicit-multiply rule.
            if (c.isDigit() || (c == '.' && i + 1 < s.size() && s[i + 1].isDigit())) {
                int start = i;
                while (i < s.size() && s[i].isDigit()) ++i;
                if (i < s.size() && s[i] == '.') { ++i; while (i < s.size() && s[i].isDigit()) ++i; }
                if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
                    int j = i + 1;
                    if (j < s.size() && (s[j] == '+' || s[j] == '-')) ++j;
                    if (j < s.size() && s[j].isDigit()) { i = j + 1; while (i < s.size() && s[i].isDigit()) ++i; }
                }
                r += QStringView(s).mid(start, i - start);
                // Implicit multiply: number followed by letter or '('
                if (i < s.size() && (s[i].isLetter() || s[i] == '(')) r += '*';
                continue;
            }
            r += c;
            // Implicit multiply: `)` followed by letter/`(`
            if (i + 1 < s.size()) {
                QChar n = s[i + 1];
                if (c == ')' && (n.isLetter() || n == '(')) r += '*';
            }
            ++i;
        }

        BTok t(r);
        BigDec v = bExpr(t);
        t.skip();
        if (!t.atEnd()) { ok = false; return BigDec(0); }
        ok = true;
        return v;
    }
    catch (const std::exception& e)
    {
        // bigEval is tried speculatively; a parse failure here is normal control
        // flow, so it stays quiet and just reports ok = false. The message is
        // captured for callers that want to distinguish a domain error (e.g.
        // "sqrt of negative") from a plain parse failure.
        if (err) *err = QString::fromUtf8(e.what());
        ok = false;
        return BigDec(0);
    }
}

// Thin string wrapper over bigEvalValue: returns the collapsed display form.
QString BigNum::bigEval(const QString& input, bool& ok)
{
    const BigDec v = bigEvalValue(input, ok);
    return ok ? fmt(v) : QString();
}