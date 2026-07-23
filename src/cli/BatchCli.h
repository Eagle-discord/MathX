#pragma once
#include <QString>
#include <QStringList>

// ---------------------------------------------------------------------------
//  BatchCli - headless command-line batch runner
//
//  Adds the `--run-batch <file>` flag to the executable. It reads a file of
//  expressions (one per line, `#` comments), runs each straight through
//  MathEngine::evaluate() - no window, no worker thread, no event loop - prints
//  the results, and exits with a pass/fail code.
//
//  WHY this exists (see DEV_NOTES "regression suite + perf hunt"): the equality
//  feature (`2+2 = 4` -> `LHS = RHS  ✓` / `LHS ≠ RHS  ✗`) already IS an
//  assertion engine, and there is a whole in-app BatchPanel/BatchRunner for
//  running lists of expressions. This flag exposes the same run from the
//  command line so the regression suite (and CI) can execute it without a human
//  clicking through the GUI, and fail the build when any assertion breaks.
//
//  The file format matches BatchRunner::onStartClicked exactly, so a file runs
//  identically here or pasted into the in-app batch panel:
//    - one expression per line
//    - blank lines and lines starting with `#` are skipped
//    - an inline `#` and everything after it is stripped
//
//  Exit codes:  0 = every line passed
//               1 = at least one error or failed assertion
//               2 = usage / file error (nothing was run)
//
//  Usage:  MathX --run-batch tests\mathx_regression.txt
//          MathX --run-batch=suite.txt
// ---------------------------------------------------------------------------
namespace BatchCli {

// Scans the process arguments for `--run-batch <file>` / `--run-batch=<file>`.
// Returns true if the flag is present (the app should run headless and exit);
// `outPath` receives the file path, or "" if the flag was given without one.
bool parseArgs(const QStringList& args, QString& outPath);

// Runs the batch file and returns a process exit code (see codes above).
// Attaches a console on Windows so output is visible from a GUI-subsystem build.
int run(const QString& filePath);

} // namespace BatchCli
