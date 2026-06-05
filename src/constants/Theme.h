#pragma once
#include <QString>
#include <QFont>
#include <QFontDatabase>

namespace Theme {

    // Colors
    inline const QString BG = "#0b0d0c";
    inline const QString SURFACE = "#111413";
    inline const QString CARD = "#161918";
    inline const QString BORDER = "#252927";
    inline const QString ACCENT = "#00e87a";
    inline const QString ACCENT_DIM = "#00b860";
    inline const QString TEXT = "#ddeae0";
    inline const QString MUTED = "#5a6b5f";
    inline const QString DIM = "#2e3530";
    inline const QString ERROR = "#ff6b5b";
    inline const QString INFO = "#5bc8ff";
    inline const QString WARN = "#ffc44d";
    inline const QString PURPLE = "#c77dff";
    inline const QString ALG = "#ffd166";
    inline const QString FTEXT = "#ffd166";
    inline const QString HOVER = "#1d2b22";

    // Font helpers
    inline QString fontFamily() {
        static const QStringList families = { "JetBrains Mono", "Consolas", "Courier New" };
        for (const QString& f : families)
            if (QFontDatabase::families().contains(f)) return f;
        return "Courier New";
    }

    inline QFont monoFont(int pointSize, int weight = QFont::Normal) {
        QFont f(fontFamily());
        f.setPointSize(pointSize);
        f.setWeight(static_cast<QFont::Weight>(weight));
        f.setStyleHint(QFont::Monospace);
        return f;
    }

} // namespace Theme

static const QString C_BG = Theme::BG;
static const QString C_SURFACE = Theme::SURFACE;
static const QString C_CARD = Theme::CARD;
static const QString C_BORDER = Theme::BORDER;
static const QString C_ACCENT = Theme::ACCENT;
static const QString C_ACCENT_DIM = Theme::ACCENT_DIM;
static const QString C_TEXT = Theme::TEXT;
static const QString C_MUTED = Theme::MUTED;
static const QString C_ERR = Theme::ERROR;
static const QString C_VRED = "#dc1e14";
static const QString C_DRED = "#ad102f";
static const QString C_OUT = "#0d100e";   // slightly darker than surface; not in Theme
static const QString C_DIM = Theme::DIM;
static const QString C_INFO = Theme::INFO;
static const QString C_WARN = Theme::WARN;
static const QString C_PURPLE = Theme::PURPLE;
static const QString C_ALG = Theme::ALG;