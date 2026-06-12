#include <QApplication>
#include <QFontDatabase>
#include "ui/MainWindow.h"
#include "constants/Theme.h"
#include <QSurfaceFormat>
#include <QDir>

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
    QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("MATHX");
    app.setApplicationVersion("1.0");


    // -- Font loading --------------------------------------------------------------
    // Scans the :/fonts/ resource directory and loads every font it finds,
    // so you never need to manually list fonts here again — just add them to
    // your .qrc file and they'll be picked up automatically.
    bool allFontsLoaded = true;
    const QDir fontDir(":/fonts");
    const QStringList fontFiles = fontDir.entryList({ "*.ttf", "*.otf" }, QDir::Files);

    if (fontFiles.isEmpty()) {
        qWarning("MATHX: no fonts found in :/fonts/ — check your .qrc file");
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
    // automatically, so the app still runs — just with the system monospace font.

    MainWindow w;
    w.show();
    return app.exec();
}