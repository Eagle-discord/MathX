#pragma once

#ifndef NOMINMAX
#  define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/number.hpp>
#include <QString>
#include <cmath>
#include <functional>
#include <sstream>
#include <iomanip>

using BigInt = boost::multiprecision::cpp_int;
using BigDec = boost::multiprecision::number<
    boost::multiprecision::cpp_dec_float<50>>;

namespace BigNum {
    // 50-digit constants
    inline const BigDec PI("3.14159265358979323846264338327950288419716939937510");
    inline const BigDec E("2.71828182845904523536028747135266249775724709369995");
    inline const BigDec PHI("1.61803398874989484820458683436563811772030917980576");

    // Convenience wrappers
    inline BigDec sqrt(const BigDec& x) { return boost::multiprecision::sqrt(x); }
    inline BigDec acos(const BigDec& x) { return boost::multiprecision::acos(x); }
    inline BigDec asin(const BigDec& x) { return boost::multiprecision::asin(x); }
    inline BigDec sin(const BigDec& x) { return boost::multiprecision::sin(x); }
    inline BigDec cos(const BigDec& x) { return boost::multiprecision::cos(x); }
    inline BigDec tan(const BigDec& x) { return boost::multiprecision::tan(x); }
    inline BigDec log(const BigDec& x) { return boost::multiprecision::log(x); }
    inline BigDec log10(const BigDec& x) { return boost::multiprecision::log10(x); }
    inline BigDec exp(const BigDec& x) { return boost::multiprecision::exp(x); }
    inline BigDec abs(const BigDec& x) { return boost::multiprecision::abs(x); }
    inline BigDec floor(const BigDec& x) { return boost::multiprecision::floor(x); }
    inline BigDec ceil(const BigDec& x) { return boost::multiprecision::ceil(x); }
    inline BigDec pow(const BigDec& x, const BigDec& y) { return boost::multiprecision::pow(x, y); }
    QString bigPow(BigInt base, BigInt exp,
        std::function<void(int)> progressCallback = nullptr,
        const std::atomic<bool>* cancelFlag = nullptr);
    // Convert double → BigDec
    inline BigDec bd(double d) { return BigDec(d); }

    // -- Display formatting ----------------------------------------------------
    // Strip a trailing zero run after a decimal point ("1.2500" -> "1.25",
    // "3.000" -> "3") and normalise "-0" to "0". No effect on integer strings.
    inline std::string stripTrailingZeros(std::string s) {
        if (s.find('.') != std::string::npos) {
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
        }
        if (s == "-0") s = "0";
        return s;
    }

    // Expand a possibly-scientific decimal string ("1.07e+3", "1e-4") into a
    // full plain-decimal string ("1070", "0.0001"). A string with no exponent is
    // returned unchanged apart from trailing-zero cleanup.
    inline std::string expandScientific(const std::string& in) {
        const auto epos = in.find_first_of("eE");
        if (epos == std::string::npos) return stripTrailingZeros(in);
        std::string mant = in.substr(0, epos);
        const int exp = std::stoi(in.substr(epos + 1));
        bool neg = false;
        if (!mant.empty() && (mant[0] == '+' || mant[0] == '-')) {
            neg = (mant[0] == '-');
            mant = mant.substr(1);
        }
        const auto dot = mant.find('.');
        std::string digits = mant;
        int pointPos;                       // digits left of the decimal point
        if (dot == std::string::npos) {
            pointPos = static_cast<int>(mant.size());
        }
        else {
            digits = mant.substr(0, dot) + mant.substr(dot + 1);
            pointPos = static_cast<int>(dot);
        }
        pointPos += exp;
        std::string out;
        if (pointPos <= 0)
            out = "0." + std::string(-pointPos, '0') + digits;
        else if (pointPos >= static_cast<int>(digits.size()))
            out = digits + std::string(pointPos - static_cast<int>(digits.size()), '0');
        else
            out = digits.substr(0, pointPos) + "." + digits.substr(pointPos);
        return (neg ? "-" : "") + stripTrailingZeros(out);
    }

    // Collapsed display form (the default used everywhere). Exact integers below
    // 1e21 print in full; every other value uses ~15 significant figures and
    // switches to scientific automatically for very large (>=~1e15) or very small
    // (<=1e-5) magnitudes. Use fmtFull() for the fully-expanded digit string.
    inline QString fmt(const BigDec& v)
    {
        namespace bmp = boost::multiprecision;
        if (bmp::isnan(v)) return QStringLiteral("NaN");
        if (bmp::isinf(v)) return v.sign() < 0 ? QStringLiteral("-Inf") : QStringLiteral("Inf");
        if (v == 0) return QStringLiteral("0");

        const BigDec av = bmp::abs(v);
        if (v == bmp::floor(v) && av < BigDec("1e21"))
            return QString::fromStdString(stripTrailingZeros(v.str(0, std::ios_base::fixed)));

        return QString::fromStdString(
            stripTrailingZeros(v.str(15, std::ios_base::fmtflags(0))));
    }

    // Largest plain-decimal expansion fmtFull will produce. Past this the string
    // is dominated by meaningless zeros (BigDec only carries ~50 significant
    // figures), and a value like 8.9e+64985351 would otherwise materialise a
    // 65-million-character string. Beyond the cap there is nothing useful to
    // expand to, so fmtFull returns the collapsed form and the UI offers no
    // expand toggle (since full == collapsed).
    inline constexpr int kMaxExpandDigits = 1000;

    // Fully-expanded display form: all significant digits as a plain decimal,
    // with any scientific exponent expanded out. Returns the collapsed form when
    // the expansion would exceed kMaxExpandDigits (see above).
    inline QString fmtFull(const BigDec& v)
    {
        namespace bmp = boost::multiprecision;
        if (bmp::isnan(v)) return QStringLiteral("NaN");
        if (bmp::isinf(v)) return v.sign() < 0 ? QStringLiteral("-Inf") : QStringLiteral("Inf");
        if (v == 0) return QStringLiteral("0");

        const std::string s = v.str(0, std::ios_base::fmtflags(0));
        // Cheaply bound the expanded length from the exponent BEFORE building it,
        // so we never allocate a giant string just to discard it.
        const auto epos = s.find_first_of("eE");
        if (epos != std::string::npos) {
            const long exp = std::strtol(s.c_str() + epos + 1, nullptr, 10);
            if (std::labs(exp) + static_cast<long>(s.size()) > kMaxExpandDigits)
                return fmt(v);
        }
        const std::string full = expandScientific(s);
        if (static_cast<int>(full.size()) > kMaxExpandDigits)
            return fmt(v);
        return QString::fromStdString(full);
    }

    // Format a plain double
    inline QString fmtD(double v)
    {
        if (std::isnan(v))
            return QStringLiteral("NaN");
        if (std::isinf(v))
            return v < 0 ? QStringLiteral("-Inf") : QStringLiteral("Inf");

        QString s = QString::number(v, 'g', 10);

        // normalize negative zero
        if (s == "-0")
            s = "0";

        return s;
    }
    // Bignum factorial - exact integer string
    QString bigFactorial(BigInt n,
        std::function<void(int)> progressCallback = nullptr,
        std::function<void(const QString&)> phaseCallback = nullptr);

    // -- Factorial size estimate ----------------------------------------------
    // Instant estimate of how big n! is, for showing the magnitude while the
    // exact value (whose base-10 formatting is the real wait for large n) is
    // still being computed. Split into parts because the UI superscripts the
    // exponent rather than printing it inline.
    // One method's answer, so the two can be shown side by side.
    struct MethodEstimate {
        QString   mantissa;      // "8.2639"
        long long exponent = 0;  // 5565708
    };

    struct FactorialEstimate {
        bool      valid = false;
        QString   nDisplay;      // "1000000"
        QString   mantissa;      // headline (log-gamma, the more exact of the two)
        long long exponent = 0;
        QString   digits;        // "5,565,709" (grouped for reading)

        // Both methods, kept so the card can compare them rather than silently
        // picking one. Stirling is an APPROXIMATION OF log-gamma, so showing the
        // pair is the actual lesson.
        MethodEstimate stirling; // Stirling's series, with the 1/(12n) term
        MethodEstimate gamma;    // log-gamma: ln(n!) = lgamma(n+1), essentially exact
        QString   agreement;     // human-readable verdict on how close they are
    };

    // Stirling's series:  ln(n!) = n·ln n − n + ½·ln(2πn) + 1/(12n)
    // The 1/(12n) term drops the error to ~1/(360n³), which keeps the digit
    // count trustworthy — without it the count can be off by one when
    // log10(n!) happens to land just under an integer.
    FactorialEstimate factorialEstimate(const BigInt& n);

    // Estimate for `expr` when it is a bare factorial big enough to be worth
    // previewing; otherwise {valid=false}. Small factorials finish long before a
    // preview would help, so they don't get one.
    FactorialEstimate estimateForExpression(const QString& expr);

    // High-precision expression evaluator (no variables). bigEvalValue returns
    // the raw BigDec (so callers can format it as collapsed or full); bigEval is
    // the string wrapper returning the collapsed display form. If `err` is given,
    // it receives the exception message on failure (e.g. "sqrt of negative"),
    // letting callers distinguish a domain error from a plain parse failure.
    BigDec   bigEvalValue(const QString& expr, bool& ok, QString* err = nullptr);
    QString  bigEval(const QString& expr, bool& ok);
}