#pragma once
#include <QString>
#include <QFont>
#include <QFontDatabase>

// -- Theme ---------------------------------------------------------------------
// Static colors are compile-time constants.
// Dynamic values (accent, font) are functions that read from Settings at call
// time - so every call site automatically picks up runtime changes without any
// extra wiring.
//
// Rule: never capture Theme::ACCENT() into a local QString at widget-creation
// time. Always call it where the value is consumed (e.g. inside setStyleSheet).

namespace Theme {

    // -- Static colors (never change at runtime) --------------------------------
    // QLatin1StringView, not QString, and constexpr rather than const.
    //
    // These were `inline const QString`, which meant every one of them was a
    // heap-allocating object built during dynamic initialisation. Two costs:
    //
    //   1. The C_ aliases below were `static const QString`, so each of the ~90
    //      translation units constructed its OWN copy of all 17 - roughly 1400
    //      redundant QString allocations before main() ran.
    //   2. Worse, a `static` at namespace scope has ORDERED dynamic init within
    //      its TU, while an `inline` variable has UNORDERED init. Nothing in the
    //      standard sequenced `C_BG = Theme::BG` after `Theme::BG` itself, so
    //      the aliases were reading a possibly-unconstructed QString. It worked
    //      by luck, not by rule.
    //
    // QLatin1StringView is constant-initialised: no allocation, no dynamic init,
    // therefore no ordering question at all. It converts implicitly to QString
    // and Qt 6's QString::arg() takes it directly, so call sites are unchanged.
    inline constexpr QLatin1StringView BG{ "#0b0d0c" };
    inline constexpr QLatin1StringView SURFACE{ "#111413" };
    inline constexpr QLatin1StringView CARD{ "#161918" };
    inline constexpr QLatin1StringView BORDER{ "#252927" };
    inline constexpr QLatin1StringView ACCENT_DIM{ "#00b860" };  // fallback dim; prefer dimAccent()
    inline constexpr QLatin1StringView TEXT{ "#ddeae0" };
    inline constexpr QLatin1StringView MUTED{ "#5a6b5f" };
    inline constexpr QLatin1StringView DIM{ "#2e3530" };
    inline constexpr QLatin1StringView ERROR{ "#ff6b5b" };
    inline constexpr QLatin1StringView ERR = ERROR;
    inline constexpr QLatin1StringView INFO{ "#5bc8ff" };
    inline constexpr QLatin1StringView WARN{ "#ffc44d" };
    inline constexpr QLatin1StringView PURPLE{ "#c77dff" };
    inline constexpr QLatin1StringView ALG{ "#ffd166" };
    inline constexpr QLatin1StringView FTEXT{ "#ffd166" };
    inline constexpr QLatin1StringView HOVER{ "#1d2b22" };
    inline constexpr QLatin1StringView DRED{ "#950606" };

    // -- Dynamic values - read from Settings at call time ----------------------
    // Defined in Theme.cpp to avoid a circular include with Settings.h.
    QString accentColor();   // Settings::accentColor() or "#00e87a"
    QString fontFamily();    // Settings::fontFamily() with mono fallback
    int     fontSize();      // Settings::fontSize() or 10

    // Dimmed variant of the current accent - for hover states etc.
    // Computed from accentColor() so it always matches.
    QString dimAccent();

    // Convenience - use these in stylesheets instead of the raw string above.
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
// `inline constexpr`, never `static const QString`. See the note on the Theme::
// colors above: the old `static` form gave every translation unit its own
// heap-allocated copy AND created an unsequenced dependency on the Theme::
// inline variables. These are constant-initialised, so both problems are gone
// and the symbols are shared program-wide.
//
// Do NOT add C_ACCENT here as a constant; it is runtime-dependent (it reads
// Settings), so it stays the macro at the bottom of this file.
inline constexpr QLatin1StringView C_BG = Theme::BG;
inline constexpr QLatin1StringView C_SURFACE = Theme::SURFACE;
inline constexpr QLatin1StringView C_CARD = Theme::CARD;
inline constexpr QLatin1StringView C_BORDER = Theme::BORDER;
inline constexpr QLatin1StringView C_TEXT = Theme::TEXT;
inline constexpr QLatin1StringView C_MUTED = Theme::MUTED;
inline constexpr QLatin1StringView C_ERR = Theme::ERROR;
inline constexpr QLatin1StringView C_VRED{ "#dc1e14" };
inline constexpr QLatin1StringView C_DRED{ "#ad102f" };
inline constexpr QLatin1StringView C_OUT{ "#0d100e" };
inline constexpr QLatin1StringView C_DIM = Theme::DIM;
inline constexpr QLatin1StringView C_INFO = Theme::INFO;
inline constexpr QLatin1StringView C_WARN = Theme::WARN;
inline constexpr QLatin1StringView C_PURPLE = Theme::PURPLE;
inline constexpr QLatin1StringView C_ALG = Theme::ALG;
inline constexpr QLatin1StringView C_ACCENT_DIM = Theme::ACCENT_DIM;

// C_ACCENT - intentionally omitted as a const.
// Use Theme::ACCENT() everywhere instead.
// This macro lets existing C_ACCENT references keep compiling while being dynamic.
#define C_ACCENT Theme::ACCENT()