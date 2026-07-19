#pragma once
#include <QString>
#include <QMetaType>

// Strongly-typed replacement for the old result-type tag strings ("ok", "err",
// "big", ...) that used to be threaded through the whole pipeline
// (MathEngine -> worker thread -> OutputArea/BatchPanel).
//
// `none` is the default (value 0). The math engine's tryX() helpers return a
// default-constructed CalcResult (`return {};`) to mean "I didn't handle this
// expression"; the dispatcher then checks `type != ResultType::none` instead of
// the old `!type.isEmpty()`. Keeping none == 0 preserves that sentinel for free.
enum class ResultType {
    none = 0,   // default: handler produced no result / not handled
    ok,         // plain arithmetic
    err,        // error message
    geo,        // geometry card
    conv,       // unit conversion
    trig,       // trigonometric evaluation
    alg,        // algebra / equation solving
    big,        // big-number (exact) result
    func        // user-defined function definition or call
};

// Lets ResultType travel through queued (cross-thread) signal/slot connections,
// e.g. PersistentWorker::resultReady emitted from the worker thread and received
// on the UI thread. Pair this with a one-time qRegisterMetaType<ResultType>()
// call at startup (see main.cpp).
Q_DECLARE_METATYPE(ResultType)

namespace ResultTypes {

// Canonical lowercase tag for a type. Round-trips with fromString(). Useful for
// logging/debugging and any remaining string boundary (e.g. serialization).
inline QString toString(ResultType t) {
    switch (t) {
    case ResultType::ok:   return QStringLiteral("ok");
    case ResultType::err:  return QStringLiteral("err");
    case ResultType::geo:  return QStringLiteral("geo");
    case ResultType::conv: return QStringLiteral("conv");
    case ResultType::trig: return QStringLiteral("trig");
    case ResultType::alg:  return QStringLiteral("alg");
    case ResultType::big:  return QStringLiteral("big");
    case ResultType::func: return QStringLiteral("func");
    case ResultType::none: break;
    }
    return QString();
}

// Parse a legacy tag string into a ResultType. Unknown/empty maps to none.
inline ResultType fromString(const QString& s) {
    if (s == QLatin1String("ok"))   return ResultType::ok;
    if (s == QLatin1String("err"))  return ResultType::err;
    if (s == QLatin1String("geo"))  return ResultType::geo;
    if (s == QLatin1String("conv")) return ResultType::conv;
    if (s == QLatin1String("trig")) return ResultType::trig;
    if (s == QLatin1String("alg"))  return ResultType::alg;
    if (s == QLatin1String("big"))  return ResultType::big;
    if (s == QLatin1String("func")) return ResultType::func;
    return ResultType::none;
}

} // namespace ResultTypes
