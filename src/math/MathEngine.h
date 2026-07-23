#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include "../shapes/ShapeDef.h"
#include "BigNum.h"
#include "../constants/ResultTypes.h"

struct CalcResult {
    QString result;
    ResultType type = ResultType::none;   // none == "not handled" (see ResultTypes.h)
    QString formula;
};

// A user-defined single-variable function, e.g. f(x) = x + 5
// A user-defined function, e.g. f(x) = x + 5  or  g(x, y) = x^2 + y.
// Supports one or more parameters; single-parameter definitions keep working
// exactly as before.
struct FunctionDef {
    QString name;              // "f", "P", etc. (original case, as typed)
    QStringList params;        // parameter names in order, e.g. {"x"} or {"x","y"}
    QString body;              // RHS expression, e.g. "x + 5" or "x^2 + y"
    QString display;           // pretty form: "f(x) = x + 5"

    // Backward-compatible accessor: the first parameter. Existing code that
    // referred to a single varName can use params.value(0).
    QString varName() const { return params.isEmpty() ? QString() : params.first(); }
    int arity() const { return params.size(); }
};

struct UnitDef {
    QStringList names;   // all accepted spellings
    double toMeters;     // multiply by this to get SI base unit
    QString siUnit;      // display unit
};

using VarMap = QMap<QString, double>;

class MathEngine : public QObject {
    Q_OBJECT

public:
    // -- Singleton access ------------------------------------------------------
    static MathEngine* instance() {
        static MathEngine inst;
        return &inst;
    }

    // Delete copy/move so the singleton can't be duplicated
    MathEngine(const MathEngine&) = delete;
    MathEngine& operator=(const MathEngine&) = delete;

    // -- All methods stay callable as before via instance() -------------------
    static CalcResult evaluate(const QString& expr);

    static double evalSimple(const QString& expr, bool& ok);
    static double evalWith(const QString& expr, const VarMap& vars, bool& ok);

    static QString bigFactorial(int n);
    static QString bigEval(const QString& expr, bool& ok);

    static long long gcd(long long a, long long b);
    static long long lcm(long long a, long long b);
    static long long nCr(int n, int r);
    static long long nPr(int n, int r);
    static bool      solveEquation(const QString& eq, double& result);

    // -- User-defined functions: f(x) = ...  -----------------------------------
    // Query / manage functions defined so far via evaluate(), e.g. "f(x) = x + 5".
    static QStringList  definedFunctionNames();          // e.g. {"f","P"}
    static bool          isFunctionDefined(const QString& name);
    static const FunctionDef* functionDef(const QString& name); // nullptr if none
    static void          clearFunctions();

    // -- Persistent variables: x = 5, then reuse x  ----------------------------
    // Assignments made via evaluate() ("x = 5") persist and are substituted into
    // later expressions. `ans` always holds the last numeric result.
    static QStringList  definedVariableNames();          // e.g. {"x","ans"}
    static bool          isVariableDefined(const QString& name);
    static QString       variableValue(const QString& name); // display value, "" if none
    static void          clearVariables();
    // Bind a variable directly. The name may be a multi-word phrase ("apples
    // with timmy"); it is normalised the same way an assignment would.
    static void          setVariable(const QString& name, const QString& value);

signals:
    // Emitted by tryAlgebra when a simplifiable form is detected.
    // 'before' is the raw input, 'after' is the list of simplified forms.
    void simplification(const QString& before, const QStringList& after);
    void factorial(QString expr);
private:
    // Private constructor - only instance() can create it
    explicit MathEngine(QObject* parent = nullptr) : QObject(parent) {}

    static CalcResult tryConversion(const QString& expr);
    static CalcResult tryGeometry(const QString& expr);
    static CalcResult tryTrig(const QString& expr);
    static double evalSide(const QString& side, const QString& varName, double val);
    static CalcResult tryAlgebra(const QString& expr);
    static CalcResult tryArithmetic(const QString& expr);
    static CalcResult tryFunctionDefinition(const QString& expr);
    static CalcResult tryFunctionCall(const QString& expr);
    static CalcResult tryFactor(const QString& expr);
    static CalcResult trySystem(const QString& expr);
    static CalcResult tryAssignment(const QString& expr);
    // Multi-word named quantities ("no. of apples with timmy = 5"). Runs on the
    // raw input before natural-language preprocessing - see the .cpp for why.
    static CalcResult tryPhraseAssignment(const QString& expr);
    static CalcResult tryStatements(const QString& expr);
    static CalcResult tryConditional(const QString& expr);
    static CalcResult tryWhere(const QString& expr);

    // The real dispatcher. evaluate() wraps this to record `ans`.
    static CalcResult dispatchEvaluate(const QString& expr);
};