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

using BigInt = boost::multiprecision::cpp_int;
using BigDec = boost::multiprecision::number<
                   boost::multiprecision::cpp_dec_float<50>>;

namespace BigNum {
    // 50-digit constants
    inline const BigDec PI ("3.14159265358979323846264338327950288419716939937510");
    inline const BigDec E  ("2.71828182845904523536028747135266249775724709369995");
    inline const BigDec PHI("1.61803398874989484820458683436563811772030917980576");

    // Convenience wrappers
    inline BigDec sqrt (const BigDec& x) { return boost::multiprecision::sqrt(x); }
    inline BigDec acos (const BigDec& x) { return boost::multiprecision::acos(x); }
    inline BigDec asin (const BigDec& x) { return boost::multiprecision::asin(x); }
    inline BigDec sin  (const BigDec& x) { return boost::multiprecision::sin(x); }
    inline BigDec cos  (const BigDec& x) { return boost::multiprecision::cos(x); }
    inline BigDec tan  (const BigDec& x) { return boost::multiprecision::tan(x); }
    inline BigDec log  (const BigDec& x) { return boost::multiprecision::log(x); }
    inline BigDec log10(const BigDec& x) { return boost::multiprecision::log10(x); }
    inline BigDec exp  (const BigDec& x) { return boost::multiprecision::exp(x); }
    inline BigDec abs  (const BigDec& x) { return boost::multiprecision::abs(x); }
    inline BigDec floor(const BigDec& x) { return boost::multiprecision::floor(x); }
    inline BigDec ceil (const BigDec& x) { return boost::multiprecision::ceil(x); }
    inline BigDec pow  (const BigDec& x, const BigDec& y) { return boost::multiprecision::pow(x, y); }

    // Convert double → BigDec
    inline BigDec bd(double d) { return BigDec(d); }

    // Format for display: double-precision output, strip trailing zeros
    inline QString fmt(const BigDec& v) {
        double d = static_cast<double>(v);
        if (!std::isfinite(d)) return "\xe2\x80\x94";
        QString s = QString::number(d, 'f', 10);
        if (s.contains('.')) {
            while (s.endsWith('0')) s.chop(1);
            if (s.endsWith('.'))   s.chop(1);
        }
        return s;
    }

    // Format a plain double
    inline QString fmtD(double v) {
        if (!std::isfinite(v)) return "\xe2\x80\x94";
        QString s = QString::number(v, 'g', 10);
        return s;
    }

    // Bignum factorial — exact integer string
    QString bigFactorial(BigInt n, std::function<void(int)> progressCallback = nullptr);

    // High-precision expression evaluator (no variables)
    QString bigEval(const QString& expr, bool& ok);
}
