#include <math/MathEngine.h>
#include <QApplication>
#include <QFontDatabase>
#include "ui/MainWindow.h"
#include "constants/Theme.h"
#include <QSurfaceFormat>
#include <QDir>
#include <sstream>
#include <iomanip>
#include <math/NaturalLanguage.h>
#include <QTimer>
#include "constants/ResultTypes.h"
#include "cli/BatchCli.h"
int main(int argc, char* argv[]) {
    // ResultType travels through a queued (cross-thread) signal
    // (PersistentWorker::resultReady, emitted on the worker thread). Queued
    // connections marshal arguments by metatype, so register it before any
    // worker thread starts.
    qRegisterMetaType<ResultType>("ResultType");

    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("MATHX");
    app.setApplicationVersion("1.0");

    // -- Headless batch mode: `--run-batch <file>` --------------------------------
    // Run a file of expressions straight through the math engine and exit with a
    // pass/fail code - no window, no worker thread, no event loop. Meant for the
    // regression suite (see DEV_NOTES) and CI. Checked here, before the GUI is
    // built, so nothing renders. See cli/BatchCli.h for the format and exit codes.
    {
        QString batchPath;
        if (BatchCli::parseArgs(app.arguments(), batchPath))
            return BatchCli::run(batchPath);
    }


    // -- Font loading --------------------------------------------------------------
    // Scans the :/fonts/ resource directory and loads every font it finds,
    // so you never need to manually list fonts here again - just add them to
    // your .qrc file and they'll be picked up automatically.
    bool allFontsLoaded = true;
    const QDir fontDir(":/fonts");
    const QStringList fontFiles = fontDir.entryList({ "*.ttf", "*.otf" }, QDir::Files);

    if (fontFiles.isEmpty()) {
        qWarning("MATHX: no fonts found in :/fonts/ - check your .qrc file");
        allFontsLoaded = false;
    }

    for (const QString& fileName : fontFiles) {
        const QString path = ":/fonts/" + fileName;
        if (QFontDatabase::addApplicationFont(path) == -1) {
            qWarning("MATHX: failed to load font %s", qPrintable(path));
            allFontsLoaded = false;
        }
    }

    if (allFontsLoaded) {
        QFont f(Theme::fontFamily());
        f.setPointSize(10);
        app.setFont(f);
    }
    // If fonts failed, Theme::fontFamily() falls back to Consolas / Courier New
    // automatically, so the app still runs - just with the system monospace font.


    MainWindow w;
    w.show();
    QTimer::singleShot(0, []() {
        NaturalLanguage::warmUp();
        });
    QTimer::singleShot(0, []() {
        NaturalLanguage::warmUp();
        bool ok;
        MathEngine::evaluate("2+2");           // Expr + BigDec paths
        MathEngine::evaluate("1 km to m");     // builds the unit DB
        MathEngine::evaluate("x = 1");         // assignment path
        MathEngine::clearVariables();          // undo the side effect
        });
    return app.exec();
}