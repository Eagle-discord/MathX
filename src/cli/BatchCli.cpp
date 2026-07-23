#include "BatchCli.h"

#include "../math/MathEngine.h"
#include "../math/BigNum.h"
#include "../math/NaturalLanguage.h"
#include "../constants/ResultTypes.h"

#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QChar>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <cstdio>
#endif

namespace {

// The pass / fail glyphs the equality engine prints (MathEngine.cpp). A failing
// assertion is still ResultType::ok - only the text carries the ✗ - so pass/fail
// detection MUST scan the result string, not the type. (See DEV_NOTES: "Scan the
// output for X".)
constexpr char16_t CHECK = u'✓';  // ✓
constexpr char16_t CROSS = u'✗';  // ✗

// A WIN32-subsystem executable (add_executable(... WIN32 ...)) has no console,
// so stdout/stderr go nowhere by default.
//
// If stdout is ALREADY redirected - `MathX --run-batch f > out.txt`, a CI pipe,
// or Start-Process -RedirectStandardOutput - the inherited handle is valid and
// we must leave it alone; reopening CONOUT$ would clobber the redirection and
// send everything to the terminal instead of the file. Only when there is no
// stdout at all (the app was launched straight from a terminal) do we attach the
// parent console and wire the CRT streams to it.
void attachConsole() {
#ifdef Q_OS_WIN
    const HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const bool redirected = (hOut != nullptr && hOut != INVALID_HANDLE_VALUE);
    if (redirected) return;   // keep the file/pipe the parent set up

    bool attached = AttachConsole(ATTACH_PARENT_PROCESS);
    if (!attached) attached = AllocConsole();
    if (attached) {
        FILE* f = nullptr;
        freopen_s(&f, "CONOUT$", "w", stdout);
        freopen_s(&f, "CONOUT$", "w", stderr);
        freopen_s(&f, "CONIN$",  "r", stdin);
        SetConsoleOutputCP(CP_UTF8);   // render ✓ / ✗ / ≠ correctly
    }
#endif
}

// Evaluate one line exactly the way the interactive worker does
// (PersistentWorker::process): exact BigNum for standalone `n!` / `fact(n)` and
// `a^b`, everything else through MathEngine::evaluate. Mirroring the worker
// means a batch result is identical to what the user would see if they typed the
// same line into the app - minus the progress bar and the Stop button. Thrown
// exceptions (unparseable input, undefined variable) become an err result, just
// as the worker's catch does.
CalcResult evalOne(const QString& expr) {
    static const QRegularExpression factRe(
        R"(^\s*(?:fact\s*\(\s*(\d+)\s*\)|(\d+)\s*!)\s*$)");
    if (auto m = factRe.match(expr); m.hasMatch()) {
        const QString nStr = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        BigInt n(nStr.toStdString());
        if (n < 0) return { "Negative factorial", ResultType::err };
        try { return { BigNum::bigFactorial(n), ResultType::big }; }
        catch (const std::exception& e) { return { QString("Error: ") + e.what(), ResultType::err }; }
    }

    static const QRegularExpression powRe(R"(^\s*(\d+)\s*\^\s*(\d+)\s*$)");
    if (auto m = powRe.match(expr); m.hasMatch()) {
        BigInt base(m.captured(1).toStdString());
        BigInt exp(m.captured(2).toStdString());
        if (exp < 0) return { "Negative exponent not supported", ResultType::err };
        try { return { BigNum::bigPow(base, exp), ResultType::big }; }
        catch (const std::exception& e) { return { QString("Error: ") + e.what(), ResultType::err }; }
    }

    try { return MathEngine::evaluate(expr); }
    catch (const std::exception& e) { return { QString("Error: ") + e.what(), ResultType::err }; }
}

// Read every line of the file into a list.
bool readLines(const QString& path, QStringList& out, QString& errMsg) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errMsg = f.errorString();
        return false;
    }
    QTextStream in(&f);
    in.setEncoding(QStringConverter::Utf8);
    while (!in.atEnd()) out << in.readLine();
    return true;
}

// Same parse as BatchRunner::onStartClicked so a file behaves identically here
// and in the in-app batch panel.
QStringList parseExpressions(const QStringList& lines) {
    QStringList exprs;
    for (const QString& line : lines) {
        QString t = line.trimmed();
        if (t.isEmpty() || t.startsWith('#')) continue;
        const int comment = t.indexOf('#');
        if (comment > 0) t = t.left(comment).trimmed();
        if (!t.isEmpty()) exprs << t;
    }
    return exprs;
}

} // namespace

namespace BatchCli {

bool parseArgs(const QStringList& args, QString& outPath) {
    static const QString flag = QStringLiteral("--run-batch");
    static const QString flagEq = QStringLiteral("--run-batch=");
    for (int i = 1; i < args.size(); ++i) {
        const QString& a = args[i];
        if (a == flag) {
            outPath = (i + 1 < args.size()) ? args[i + 1] : QString();
            return true;
        }
        if (a.startsWith(flagEq)) {
            outPath = a.mid(flagEq.size());
            return true;
        }
    }
    return false;
}

int run(const QString& filePath) {
    attachConsole();

    QTextStream out(stdout);
    QTextStream err(stderr);
    out.setEncoding(QStringConverter::Utf8);
    err.setEncoding(QStringConverter::Utf8);

    if (filePath.isEmpty()) {
        err << "mathx: --run-batch requires a file path\n"
               "usage: MathX --run-batch <file>\n";
        return 2;
    }

    QStringList lines;
    QString ioError;
    if (!readLines(filePath, lines, ioError)) {
        err << "mathx: cannot open '" << filePath << "': " << ioError << "\n";
        return 2;
    }

    const QStringList exprs = parseExpressions(lines);

    // Preload the natural-language tables once (main.cpp does the same on
    // startup); avoids first-call latency landing on line 1.
    NaturalLanguage::warmUp();

    const QString rule(60, QChar('='));
    const QString thin(60, QChar('-'));

    out << "MATHX batch - " << filePath
        << "  (" << exprs.size() << " expression" << (exprs.size() == 1 ? "" : "s") << ")\n"
        << rule << "\n";

    int passed = 0, failedAssert = 0, errors = 0;
    for (int i = 0; i < exprs.size(); ++i) {
        const QString& e = exprs.at(i);
        const CalcResult r = evalOne(e);

        const bool isErr  = (r.type == ResultType::err);
        const bool isFail = r.result.contains(QChar(CROSS));   // failed assertion
        const QChar glyph = isErr ? QChar('!')
                          : isFail ? QChar(CROSS)
                                   : QChar(CHECK);

        out << glyph << " [" << (i + 1) << '/' << exprs.size() << "] " << e << "\n";

        // Success is quiet; only expand the full result for a failure/error so
        // the reason is visible without drowning a passing run in output.
        if (isErr || isFail) {
            const QStringList body = r.result.split('\n');
            for (const QString& bl : body)
                out << "        " << bl << "\n";
        }

        if (isErr)       ++errors;
        else if (isFail) ++failedAssert;
        else             ++passed;
    }

    const int failures = errors + failedAssert;
    out << thin << "\n"
        << exprs.size() << " expression" << (exprs.size() == 1 ? "" : "s") << " · "
        << passed << " ok · "
        << failedAssert << " failed · "
        << errors << " error" << (errors == 1 ? "" : "s") << "\n";
    out.flush();

    return failures == 0 ? 0 : 1;
}

} // namespace BatchCli
