#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include "..\shapes\ShapeDef.h"
#include "BigNum.h"

struct CalcResult {
    QString result;
    QString type;   // "ok","err","geo","trig","conv","alg","big"
    QString formula;
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
    // ── Singleton access ──────────────────────────────────────────────────────
    static MathEngine* instance() {
        static MathEngine inst;
        return &inst;
    }

    // Delete copy/move so the singleton can't be duplicated
    MathEngine(const MathEngine&) = delete;
    MathEngine& operator=(const MathEngine&) = delete;

    // ── All methods stay callable as before via instance() ───────────────────
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

signals:
    // Emitted by tryAlgebra when a simplifiable form is detected.
    // 'before' is the raw input, 'after' is the list of simplified forms.
    void simplification(const QString& before, const QStringList& after);
    void factorial(QString expr);
private:
    // Private constructor — only instance() can create it
    explicit MathEngine(QObject* parent = nullptr) : QObject(parent) {}
    
    static CalcResult tryConversion(const QString& expr);
    static CalcResult tryGeometry(const QString& expr);
    static CalcResult tryTrig(const QString& expr);
    static double evalSide(const QString& side, const QString& varName, double val);
    static CalcResult tryAlgebra(const QString& expr);
    static CalcResult tryArithmetic(const QString& expr);
};