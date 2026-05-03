#include <QApplication>
#include <QFontDatabase>
#include "ui/MainWindow.h"
#include "theme/Theme.h"

int main(int argc, char* argv[]) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("MATHX");
    app.setApplicationVersion("1.0");


    struct FontLoad { const char* path; const char* weight; };
    bool allFontsLoaded = true;
    for (auto& fl : { FontLoad{":/fonts/JetBrainsMono-Regular.ttf",   "Regular"},
                      FontLoad{":/fonts/JetBrainsMono-Medium.ttf",    "Medium"},
                      FontLoad{":/fonts/JetBrainsMono-Bold.ttf",      "Bold"},
                      FontLoad{":/fonts/JetBrainsMono-ExtraBold.ttf", "ExtraBold"} }) {
        if (QFontDatabase::addApplicationFont(fl.path) == -1) {
            qWarning("MATHX: failed to load font %s (%s)", fl.path, fl.weight);
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