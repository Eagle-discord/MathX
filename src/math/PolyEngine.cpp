#include "PolyEngine.h"
#include "Expr.h"
#include <QRegularExpression>
#include <QMap>
#include <QSet>
#include <QList>
#include <QStringList>
#include <QPair>
#include <algorithm>
#include <cmath>
#include <complex>

// ===========================================================================
//  Poly member functions
// ===========================================================================
void Poly::trim() {
    while (coeffs.size() > 1 && coeffs.back() == 0)
        coeffs.pop_back();
    if (coeffs.isEmpty()) coeffs.append(BigDec(0));
}

Poly Poly::operator+(const Poly& o) const {
    int n = std::max(coeffs.size(), o.coeffs.size());
    QVector<BigDec> r(n, BigDec(0));
    for (int i = 0; i < n; ++i) r[i] = at(i) + o.at(i);
    return Poly(r, var);
}
Poly Poly::operator-(const Poly& o) const {
    int n = std::max(coeffs.size(), o.coeffs.size());
    QVector<BigDec> r(n, BigDec(0));
    for (int i = 0; i < n; ++i) r[i] = at(i) - o.at(i);
    return Poly(r, var);
}
Poly Poly::operator*(const Poly& o) const {
    if (isZero() || o.isZero()) return Poly(QVector<BigDec>{BigDec(0)}, var);
    QVector<BigDec> r(coeffs.size() + o.coeffs.size() - 1, BigDec(0));
    for (int i = 0; i < coeffs.size(); ++i)
        for (int j = 0; j < o.coeffs.size(); ++j)
            r[i + j] += coeffs[i] * o.coeffs[j];
    return Poly(r, var);
}
Poly Poly::scaled(const BigDec& k) const {
    QVector<BigDec> r = coeffs;
    for (BigDec& c : r) c *= k;
    return Poly(r, var);
}
Poly Poly::derivative() const {
    if (coeffs.size() <= 1) return Poly(QVector<BigDec>{BigDec(0)}, var);
    QVector<BigDec> r(coeffs.size() - 1);
    for (int i = 1; i < coeffs.size(); ++i) r[i - 1] = coeffs[i] * BigDec(i);
    return Poly(r, var);
}

QString Poly::fmtCoeff(const BigDec& v) {
    return BigNum::fmt(v);
}

QString Poly::toString() const {
    QString out;
    bool first = true;
    for (int i = coeffs.size() - 1; i >= 0; --i) {
        BigDec c = coeffs[i];
        if (c == 0) continue;

        bool neg = (c < 0);
        BigDec mag = neg ? -c : c;

        if (first) { if (neg) out += "-"; }
        else { out += neg ? " - " : " + "; }
        first = false;

        // coefficient (omit "1" unless it's the constant term)
        bool omitOne = (mag == 1 && i != 0);
        if (!omitOne) out += fmtCoeff(mag);

        if (i >= 1) {
            out += var;
            if (i >= 2) out += "^" + QString::number(i);
        }
    }
    return first ? "0" : out;
}

// ===========================================================================
//  Small helpers
// ===========================================================================
namespace {

    // Is this BigDec within eps of an integer? If so return that integer.
    // Note: boost cpp_dec_float supports static_cast<double> (used throughout this
    // codebase, e.g. Solver.cpp) but not a direct cast to long long, so we round
    // via floor and convert through double. We cap at 2^53 so the double carries
    // the integer exactly - rational-root search never needs larger constants.
    bool nearInteger(const BigDec& v, long long& out, const BigDec& eps = BigDec("1e-9")) {
        BigDec r = BigNum::floor(v + BigDec("0.5"));
        if (BigNum::abs(v - r) < eps) {
            if (BigNum::abs(r) < BigDec("9.007199254740992e15")) {   // 2^53
                out = static_cast<long long>(static_cast<double>(r));
                return true;
            }
        }
        return false;
    }

    // Round a BigDec to the nearest integer when it's very close (keeps output clean).
    BigDec snapInteger(const BigDec& v, const BigDec& eps = BigDec("1e-9")) {
        long long n;
        if (nearInteger(v, n, eps)) return BigDec(n);
        return v;
    }

    long long llgcd(long long a, long long b) {
        a = std::llabs(a); b = std::llabs(b);
        while (b) { a %= b; std::swap(a, b); }
        return a;
    }

    // Divisors (positive) of |n|, for the rational-root search.
    QVector<long long> divisors(long long n) {
        n = std::llabs(n);
        QVector<long long> ds;
        if (n == 0) { ds.append(0); return ds; }
        for (long long i = 1; i * i <= n; ++i) {
            if (n % i == 0) {
                ds.append(i);
                if (i != n / i) ds.append(n / i);
            }
        }
        std::sort(ds.begin(), ds.end());
        return ds;
    }

    // Synthetic division of p by (x - root). Returns the quotient polynomial and
    // sets `exact` if the remainder is ~0 (i.e. root is genuinely a root).
    //
    // Horner-style synthetic division, high-order coefficient first:
    //   b_{n-1} = a_n
    //   b_{k-1} = a_k + root * b_k     (k = n-1 .. 1)
    //   remainder = a_0 + root * b_0
    Poly deflate(const Poly& p, const BigDec& root, bool& exact) {
        int n = p.coeffs.size();
        if (n <= 1) { exact = true; return Poly(QVector<BigDec>{BigDec(0)}, p.var); }

        QVector<BigDec> q(n - 1, BigDec(0));
        BigDec b = p.coeffs[n - 1];      // highest quotient coefficient
        q[n - 2] = b;
        for (int k = n - 2; k >= 1; --k) {
            b = p.coeffs[k] + root * b;
            q[k - 1] = b;
        }
        // Remainder - checked independently via eval for numerical stability.
        BigDec rem = p.eval(root);
        exact = (BigNum::abs(rem) < BigDec("1e-7"));
        return Poly(q, p.var);
    }

}  // anonymous namespace

// ===========================================================================
//  Parsing:  "5x^2 + 1"  ->  Poly{coeffs=[1,0,5]}
// ===========================================================================
bool PolyEngine::parse(const QString& expr, Poly& out) {
    QString s = expr;
    s.remove(' ');
    if (s.isEmpty()) return false;

    // Defensive superscript handling (upstream usually does this already)
    s.replace("²", "^2");
    s.replace("³", "^3");
    s.replace("⁴", "^4");

    // Reject anything with function-like letters runs or a second variable.
    // First, find the single variable.
    QSet<QString> vars = Expr::detectVariables(s);
    if (vars.size() > 1) return false;               // multivariable - not ours
    QString var = vars.isEmpty() ? QString("x") : *vars.begin();

    // If there are letters that aren't the variable, it's a function etc.
    for (const QChar& ch : s) {
        if (ch.isLetter() && QString(ch).toLower() != var) return false;
    }

    // Split into signed terms at top level (no parentheses expected here;
    // callers pass already-flat polynomials).
    if (s.contains('(') || s.contains(')')) return false;

    // Insert explicit '+' so we can split uniformly, but not for exponent signs
    // (there shouldn't be any here) - we split on + / - that start a term.
    QList<QString> terms;
    QString cur;
    for (int i = 0; i < s.size(); ++i) {
        QChar ch = s[i];
        if ((ch == '+' || ch == '-') && i > 0) {
            // Not an exponent sign (we don't allow x^-2 in this simple parser)
            terms.append(cur);
            cur = ch;
        }
        else {
            cur += ch;
        }
    }
    if (!cur.isEmpty()) terms.append(cur);

    QMap<int, BigDec> byPow;   // power -> coefficient
    // Allow an optional '*' between coefficient and variable - Expr::preprocess
    // turns "5x" into "5*x", and without this the term wouldn't match and the
    // whole polynomial would fail to parse (so factor() reported "cannot factor").
    QRegularExpression termRe(
        QString(R"(^([+-]?\d*\.?\d*)\*?(%1)?(?:\^(\d+))?$)").arg(QRegularExpression::escape(var)));

    for (QString term : terms) {
        if (term.isEmpty()) continue;
        auto m = termRe.match(term);
        if (!m.hasMatch()) return false;

        QString coeffStr = m.captured(1);
        bool hasVar = !m.captured(2).isEmpty();
        QString powStr = m.captured(3);

        BigDec coeff;
        if (coeffStr.isEmpty() || coeffStr == "+")      coeff = BigDec(1);
        else if (coeffStr == "-")                        coeff = BigDec(-1);
        else {
            try { coeff = BigDec(coeffStr.toStdString()); }
            catch (...) { return false; }
        }

        int pow = 0;
        if (hasVar) pow = powStr.isEmpty() ? 1 : powStr.toInt();

        byPow[pow] += coeff;
    }

    if (byPow.isEmpty()) return false;
    int maxPow = 0;
    for (auto it = byPow.cbegin(); it != byPow.cend(); ++it)
        maxPow = std::max(maxPow, it.key());

    QVector<BigDec> coeffs(maxPow + 1, BigDec(0));
    for (auto it = byPow.cbegin(); it != byPow.cend(); ++it)
        coeffs[it.key()] = it.value();

    out = Poly(coeffs, var);
    return true;
}

// ===========================================================================
//  Factoring via rational-root theorem + deflation
// ===========================================================================
QString PolyEngine::factor(const Poly& pIn) {
    Poly p = pIn;
    p.trim();
    int deg = p.degree();
    if (deg < 2) return {};   // nothing meaningful to factor

    // Work with integer coefficients: find a common scale.
    // Convert coeffs to nearest integers; bail if any isn't near-integer.
    QVector<long long> ic(p.coeffs.size());
    for (int i = 0; i < p.coeffs.size(); ++i) {
        long long v;
        if (!nearInteger(p.coeffs[i], v)) return {};   // non-integer coeffs: skip factoring
        ic[i] = v;
    }

    // Pull out an overall integer content (GCD of coefficients).
    long long content = 0;
    for (long long v : ic) content = llgcd(content, v);
    if (content == 0) return {};
    // Keep sign of leading coeff positive for the factored form.
    if (ic.back() < 0) content = -content;
    for (long long& v : ic) v /= content;

    // Rebuild a BigDec working polynomial from the primitive integer part.
    QVector<BigDec> prim(ic.size());
    for (int i = 0; i < ic.size(); ++i) prim[i] = BigDec(ic[i]);
    Poly work(prim, p.var);

    QStringList factors;
    // Extract rational roots p/q where p | constant, q | leading.
    bool progress = true;
    while (work.degree() >= 1 && progress) {
        progress = false;
        long long a0 = 0, an = 0;
        (void)nearInteger(work.coeffs.front(), a0);
        (void)nearInteger(work.coeffs.back(), an);

        if (an == 0) break;

        // If constant term is 0, x is a factor.
        if (a0 == 0) {
            factors << work.var;                 // factor of "x"
            // divide by x  => drop lowest coefficient
            QVector<BigDec> q = work.coeffs.mid(1);
            work = Poly(q, work.var);
            progress = true;
            continue;
        }

        QVector<long long> ps = divisors(a0);
        QVector<long long> qs = divisors(an);
        bool found = false;
        for (long long qd : qs) {
            for (long long pd : ps) {
                for (int sign : {1, -1}) {
                    if (qd == 0) continue;
                    BigDec root = BigDec(sign * pd) / BigDec(qd);
                    if (BigNum::abs(work.eval(root)) < BigDec("1e-9")) {
                        // Found rational root p/q -> factor (qx - p)  (up to sign)
                        long long P = sign * pd, Q = qd;
                        long long g = llgcd(P, Q);
                        if (g > 1) { P /= g; Q /= g; }
                        if (Q < 0) { Q = -Q; P = -P; }

                        QString fac;
                        if (Q == 1) {
                            // (x - P)
                            if (P == 0)      fac = work.var;
                            else if (P > 0)  fac = QString("(%1 - %2)").arg(work.var).arg(P);
                            else             fac = QString("(%1 + %2)").arg(work.var).arg(-P);
                        }
                        else {
                            // (Qx - P)
                            QString lead = QString("%1%2").arg(Q).arg(work.var);
                            if (P > 0)  fac = QString("(%1 - %2)").arg(lead).arg(P);
                            else if (P < 0) fac = QString("(%1 + %2)").arg(lead).arg(-P);
                            else fac = QString("(%1)").arg(lead);
                        }
                        factors << fac;

                        bool exact;
                        work = deflate(work, root, exact);
                        work.trim();
                        found = progress = true;
                        break;
                    }
                }
                if (found) break;
            }
            if (found) break;
        }
        if (!found) break;
    }

    // Whatever remains (degree >= 1 with no rational roots, or an irreducible
    // quadratic) is appended as-is.
    if (work.degree() >= 1) {
        // Only append if it isn't just the constant 1
        if (!(work.degree() == 0 && work.coeffs.front() == 1))
            factors << QString("(%1)").arg(work.toString());
    }
    else {
        // Degree 0 leftover multiplies into the content.
        long long leftover;
        if (nearInteger(work.coeffs.front(), leftover) && leftover != 1)
            content *= leftover;
    }

    if (factors.size() < 1) return {};
    // If we only peeled one linear factor off a quadratic, that's still 2 factors
    // total once the remainder is included - so require the whole thing to have
    // actually split (more than one factor, or a content != 1 in front).
    if (factors.size() == 1 && content == 1) return {};

    QString prefix = (content == 1) ? QString()
        : (content == -1 ? QString("-") : QString::number(content));
    return prefix + factors.join("");
}

// ===========================================================================
//  Root finding
// ===========================================================================
static QStringList solveLinearPoly(const Poly& p) {
    // a1 x + a0 = 0
    BigDec a1 = p.at(1), a0 = p.at(0);
    if (a1 == 0) {
        return { a0 == 0 ? QStringLiteral("Infinite solutions")
                         : QStringLiteral("No solution") };
    }
    BigDec x = snapInteger(-a0 / a1);
    return { QString("%1 = %2").arg(p.var, BigNum::fmt(x)) };
}

static QStringList solveQuadraticPoly(const Poly& p) {
    BigDec a = p.at(2), b = p.at(1), c = p.at(0);
    BigDec disc = b * b - 4 * a * c;
    QString v = p.var;

    if (disc > 0) {
        BigDec sq = BigNum::sqrt(disc);
        BigDec x1 = snapInteger((-b + sq) / (2 * a));
        BigDec x2 = snapInteger((-b - sq) / (2 * a));
        return { QString("%1\u2081 = %2").arg(v, BigNum::fmt(x1)),
                 QString("%1\u2082 = %2").arg(v, BigNum::fmt(x2)) };
    }
    else if (disc == 0) {
        BigDec x = snapInteger(-b / (2 * a));
        return { QString("%1 = %2 (double root)").arg(v, BigNum::fmt(x)) };
    }
    else {
        BigDec re = snapInteger(-b / (2 * a));
        BigDec im = snapInteger(BigNum::sqrt(-disc) / (2 * a));
        return { QString("%1\u2081 = %2 + %3i").arg(v, BigNum::fmt(re), BigNum::fmt(im)),
                 QString("%1\u2082 = %2 - %3i").arg(v, BigNum::fmt(re), BigNum::fmt(im)) };
    }
}

// Durand-Kerner: find all complex roots of a polynomial numerically.
// Used only for the non-rational remainder of degree >= 3. Operates on the
// monic form of the polynomial (in double precision, which is plenty for the
// approximate answers we label with "≈").
static QVector<std::complex<double>> durandKerner(const Poly& p) {
    int n = p.degree();
    double lead = static_cast<double>(p.at(n));

    // Normalised (monic) coefficients, low-order first.
    QVector<std::complex<double>> a(n + 1);
    for (int i = 0; i <= n; ++i)
        a[i] = std::complex<double>(static_cast<double>(p.at(i)) / lead, 0.0);

    auto evalMonic = [&](std::complex<double> x) {
        std::complex<double> r(0.0, 0.0);
        for (int i = n; i >= 0; --i) r = r * x + a[i];
        return r;
        };

    // Classic spread-out starting guesses: powers of 0.4 + 0.9i.
    QVector<std::complex<double>> roots(n);
    std::complex<double> seed(0.4, 0.9);
    std::complex<double> cur(1.0, 0.0);
    for (int i = 0; i < n; ++i) { cur *= seed; roots[i] = cur; }

    for (int iter = 0; iter < 500; ++iter) {
        double maxDelta = 0.0;
        for (int i = 0; i < n; ++i) {
            std::complex<double> num = evalMonic(roots[i]);
            std::complex<double> den(1.0, 0.0);
            for (int j = 0; j < n; ++j)
                if (j != i) den *= (roots[i] - roots[j]);
            if (std::abs(den) < 1e-14) den = std::complex<double>(1e-14, 0);
            std::complex<double> delta = num / den;
            roots[i] -= delta;
            maxDelta = std::max(maxDelta, std::abs(delta));
        }
        if (maxDelta < 1e-12) break;
    }
    return roots;
}

QStringList PolyEngine::solveRoots(const Poly& pIn) {
    Poly p = pIn;
    p.trim();
    int deg = p.degree();

    if (deg == 0) {
        return { p.at(0) == 0 ? QStringLiteral("Infinite solutions")
                              : QStringLiteral("No solution") };
    }
    if (deg == 1) return solveLinearPoly(p);
    if (deg == 2) return solveQuadraticPoly(p);

    // Degree >= 3: peel off rational roots, drop to quadratic/linear exactly
    // if possible, otherwise finish numerically.
    QStringList lines;
    Poly work = p;

    // Try integer/rational roots first (keeps nice exact answers).
    bool peeled = true;
    while (work.degree() >= 3 && peeled) {
        peeled = false;
        long long a0, an;
        if (!nearInteger(work.at(0), a0) || !nearInteger(work.at(work.degree()), an))
            break;
        if (a0 == 0) { // x = 0 root
            lines << QString("%1 = 0").arg(work.var);
            work = Poly(work.coeffs.mid(1), work.var);
            peeled = true;
            continue;
        }
        QVector<long long> ps = divisors(a0), qs = divisors(an);
        bool found = false;
        for (long long qd : qs) {
            for (long long pd : ps) {
                for (int sign : {1, -1}) {
                    if (qd == 0) continue;
                    BigDec root = BigDec(sign * pd) / BigDec(qd);
                    if (BigNum::abs(work.eval(root)) < BigDec("1e-9")) {
                        lines << QString("%1 = %2").arg(work.var, BigNum::fmt(snapInteger(root)));
                        bool ex; work = deflate(work, root, ex); work.trim();
                        found = peeled = true;
                        break;
                    }
                }
                if (found) break;
            }
            if (found) break;
        }
    }

    // Finish exactly if we reduced to <= quadratic.
    if (work.degree() == 2) { lines += solveQuadraticPoly(work); return lines; }
    if (work.degree() == 1) { lines += solveLinearPoly(work);    return lines; }
    if (work.degree() == 0) return lines;

    // Still cubic or higher with no rational roots: go numeric.
    QVector<std::complex<double>> roots = durandKerner(work);
    // Sort real-ish roots first, by real part.
    std::sort(roots.begin(), roots.end(),
        [](const std::complex<double>& x, const std::complex<double>& y) {
            bool xr = std::abs(x.imag()) < 1e-7, yr = std::abs(y.imag()) < 1e-7;
            if (xr != yr) return xr;                 // real roots first
            return x.real() < y.real();
        });

    int idx = 1;
    for (const auto& r : roots) {
        double re = r.real(), im = r.imag();
        // snap tiny values
        if (std::abs(re) < 1e-9) re = 0;
        if (std::abs(im) < 1e-7) {
            double rr = std::round(re);
            if (std::abs(re - rr) < 1e-6) re = rr;
            lines << QString("%1%2 \u2248 %3").arg(work.var).arg(idx).arg(re, 0, 'g', 8);
        }
        else {
            lines << QString("%1%2 \u2248 %3 %4 %5i")
                .arg(work.var).arg(idx)
                .arg(re, 0, 'g', 8)
                .arg(im < 0 ? "-" : "+")
                .arg(std::abs(im), 0, 'g', 8);
        }
        ++idx;
    }
    return lines;
}

// ===========================================================================
//  Linear systems (2-3 unknowns) via Gaussian elimination on BigDec
// ===========================================================================
namespace {

    // Parse one linear equation into a coefficient row over the given variable
    // ordering. Returns false if the equation isn't linear in those variables.
    // row has size vars.size()+1; last entry is the RHS constant (moved over).
    bool parseLinearRow(const QString& lhs, const QString& rhs,
        const QStringList& varsOrder, QVector<BigDec>& row) {
        row = QVector<BigDec>(varsOrder.size() + 1, BigDec(0));

        // f(vars) = lhs - rhs ; constant term goes to RHS with sign flip.
        // We recover each variable's coefficient by sampling: coefficient of v_k
        // is f(with v_k=1, others=0) - f(all zero). Constant is f(all zero).
        auto evalAt = [&](const QMap<QString, double>& vals, bool& ok) -> double {
            VarMap vm;
            for (auto it = vals.cbegin(); it != vals.cend(); ++it) vm[it.key()] = it.value();
            bool okL, okR;
            double L = Expr::evalWith(lhs, vm, okL);
            double R = Expr::evalWith(rhs, vm, okR);
            ok = okL && okR;
            return L - R;
            };

        QMap<QString, double> zero;
        for (const QString& v : varsOrder) zero[v] = 0.0;
        bool ok;
        double f0 = evalAt(zero, ok);
        if (!ok) return false;

        for (int k = 0; k < varsOrder.size(); ++k) {
            QMap<QString, double> one = zero;
            one[varsOrder[k]] = 1.0;
            double fk = evalAt(one, ok);
            if (!ok) return false;
            double coeff = fk - f0;

            // Linearity check: sample at 2 to be sure it isn't quadratic.
            QMap<QString, double> two = zero;
            two[varsOrder[k]] = 2.0;
            double f2 = evalAt(two, ok);
            if (!ok) return false;
            if (std::abs((f2 - f0) - 2.0 * coeff) > 1e-6) return false;  // nonlinear

            row[k] = BigDec(coeff);
        }
        // f(all zero) = constant term; equation is f = 0  =>  sum(coeff*var) = -f0
        row[varsOrder.size()] = BigDec(-f0);
        return true;
    }

}  // anonymous namespace

QStringList PolyEngine::solveLinearSystem(const QStringList& equations) {
    if (equations.size() < 2) return {};

    // Collect the full variable set across all equations.
    QSet<QString> varSet;
    QVector<QPair<QString, QString>> split;   // (lhs, rhs) per equation
    for (const QString& eq : equations) {
        int pos = eq.indexOf('=');
        if (pos < 0) return {};
        QString lhs = eq.left(pos).trimmed();
        QString rhs = eq.mid(pos + 1).trimmed();
        split.append({ lhs, rhs });
        varSet += Expr::detectVariables(lhs + " " + rhs);
    }
    QStringList vars = varSet.values();
    std::sort(vars.begin(), vars.end());
    if (vars.isEmpty() || vars.size() > 4) return {};        // keep it small
    if (equations.size() < vars.size()) return {};           // underdetermined

    // Build augmented matrix.
    QVector<QVector<BigDec>> M;
    for (const auto& sr : split) {
        QVector<BigDec> row;
        if (!parseLinearRow(sr.first, sr.second, vars, row)) return {};  // nonlinear -> bail
        M.append(row);
    }

    int n = vars.size();
    int m = M.size();
    // Gaussian elimination with partial pivoting.
    int r = 0;
    QVector<int> where(n, -1);
    for (int col = 0; col < n && r < m; ++col) {
        int piv = -1;
        BigDec best = 0;
        for (int i = r; i < m; ++i) {
            BigDec a = BigNum::abs(M[i][col]);
            if (a > best) { best = a; piv = i; }
        }
        if (piv < 0 || best < BigDec("1e-12")) continue;
        std::swap(M[piv], M[r]);
        // normalise pivot row
        BigDec pv = M[r][col];
        for (int j = col; j <= n; ++j) M[r][j] /= pv;
        // eliminate others
        for (int i = 0; i < m; ++i) {
            if (i == r) continue;
            BigDec f = M[i][col];
            if (f == 0) continue;
            for (int j = col; j <= n; ++j) M[i][j] -= f * M[r][j];
        }
        where[col] = r;
        ++r;
    }

    // Check consistency: any row 0 = nonzero?
    for (int i = 0; i < m; ++i) {
        bool allZero = true;
        for (int j = 0; j < n; ++j) if (BigNum::abs(M[i][j]) > BigDec("1e-10")) { allZero = false; break; }
        if (allZero && BigNum::abs(M[i][n]) > BigDec("1e-9"))
            return { QStringLiteral("No solution (inconsistent system)") };
    }

    // Free variables => infinite solutions.
    for (int col = 0; col < n; ++col)
        if (where[col] < 0)
            return { QStringLiteral("Infinite solutions (underdetermined)") };

    QStringList out;
    for (int col = 0; col < n; ++col) {
        BigDec val = snapInteger(M[where[col]][n]);
        out << QString("%1 = %2").arg(vars[col], BigNum::fmt(val));
    }
    return out;
}

// ===========================================================================
//  Routing helper
// ===========================================================================
bool PolyEngine::looksLikePolynomialEquation(const QString& lhs, const QString& rhs) {
    QString combined = lhs + " " + rhs;
    // Reject obvious non-polynomial functions.
    static const QStringList bad = {
        "sin","cos","tan","log","ln","sqrt","cbrt","exp","abs",
        "floor","ceil","asin","acos","atan"
    };
    QString lc = combined.toLower();
    for (const QString& b : bad) if (lc.contains(b)) return false;

    QSet<QString> vars = Expr::detectVariables(combined);
    if (vars.size() != 1) return false;

    // Try to parse both sides as polynomials.
    Poly a, b;
    QString v = *vars.begin();
    return true;  // detailed parse happens in Algebra.cpp; this is a cheap gate
}