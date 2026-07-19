#pragma once
#include <QString>
#include <QFont>
#include <QFontDatabase>

// -- Theme ---------------------------------------------------------------------
// Static colors are compile-time constants.
// Dynamic values (accent, font) are functions that read from Settings at call
// time — so every call site automatically picks up runtime changes without any
// extra wiring.
//
// Rule: never capture Theme::ACCENT() into a local QString at widget-creation
// time. Always call it where the value is consumed (e.g. inside setStyleSheet).

namespace Theme {

    // -- Static colors (never change at runtime) --------------------------------
    inline const QString BG = "#0b0d0c";
    inline const QString SURFACE = "#111413";
    inline const QString CARD = "#161918";
    inline const QString BORDER = "#252927";
    inline const QString ACCENT_DIM = "#00b860";   // fallback dim; prefer dimAccent()
    inline const QString TEXT = "#ddeae0";
    inline const QString MUTED = "#5a6b5f";
    inline const QString DIM = "#2e3530";
    inline const QString ERROR = "#ff6b5b";
    inline const QString ERR = ERROR;
    inline const QString INFO = "#5bc8ff";
    inline const QString WARN = "#ffc44d";
    inline const QString PURPLE = "#c77dff";
    inline const QString ALG = "#ffd166";
    inline const QString FTEXT = "#ffd166";
    inline const QString HOVER = "#1d2b22";
    inline const QString DRED = "#950606";

    // -- Dynamic values — read from Settings at call time ----------------------
    // Defined in Theme.cpp to avoid a circular include with Settings.h.
    QString accentColor();   // Settings::accentColor() or "#00e87a"
    QString fontFamily();    // Settings::fontFamily() with mono fallback
    int     fontSize();      // Settings::fontSize() or 10

    // Dimmed variant of the current accent — for hover states etc.
    // Computed from accentColor() so it always matches.
    QString dimAccent();

    // Convenience — use these in stylesheets instead of the raw string above.
    // ACCENT() is a function so it's always fresh; the old `inline const ACCENT`
    // was baked in at compile time and never updated.
    inline QString ACCENT() { return accentColor(); }

    // -- Font helpers ----------------------------------------------------------
    inline QFont monoFont(int pointSize = -1, int weight = QFont::Normal) {
        QFont f(fontFamily());
        f.setPointSize(pointSize > 0 ? pointSize : fontSize());
        f.setWeight(static_cast<QFont::Weight>(weight));
        f.setStyleHint(QFont::Monospace);
        return f;
    }

} // namespace Theme

// -- C_ shorthand aliases ------------------------------------------------------
// Static aliases are fine — they reference static Theme:: members.
// Do NOT add C_ACCENT here as a const QString; use Theme::ACCENT() at call sites.
static const QString C_BG = Theme::BG;
static const QString C_SURFACE = Theme::SURFACE;
static const QString C_CARD = Theme::CARD;
static const QString C_BORDER = Theme::BORDER;
static const QString C_TEXT = Theme::TEXT;
static const QString C_MUTED = Theme::MUTED;
static const QString C_ERR = Theme::ERROR;
static const QString C_VRED = "#dc1e14";
static const QString C_DRED = "#ad102f";
static const QString C_OUT = "#0d100e";
static const QString C_DIM = Theme::DIM;
static const QString C_INFO = Theme::INFO;
static const QString C_WARN = Theme::WARN;
static const QString C_PURPLE = Theme::PURPLE;
static const QString C_ALG = Theme::ALG;
static const QString C_ACCENT_DIM = Theme::ACCENT_DIM;

// C_ACCENT — intentionally omitted as a const.
// Use Theme::ACCENT() everywhere instead.
// This macro lets existing C_ACCENT references keep compiling while being dynamic.
#define C_ACCENT Theme::ACCENT()