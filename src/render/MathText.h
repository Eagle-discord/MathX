#pragma once
//
// MathText - our own character-by-character text system, built from the source
// font's actual vector outlines.
//
// The reference posters use Noto Sans (Medium/SemiBold) for annotation text
// like "cos(π)+isin(π)", and a Computer-Modern-style serif for the math
// (πr²). Instead of tracing pixels, GlyphLib harvests each character's TRUE
// Bézier outline from the font file into a QPainterPath - so every glyph
// becomes vector data WE own. The same harvester works on any font family:
// that is the "pattern that goes to other fonts".
//
// On top of the glyph library sit two orthogonal layers:
//   MATH      - Line: a tiny typesetting engine (runs, superscripts, colour
//               groups; fractions/radicals can be added the same way).
//   ANIMATION - draw(): renders placed glyphs at a progress t, with each glyph
//               tracing its own outline stroke-by-stroke and then filling in
//               (the classic hand-drawn math-animation look). Only possible
//               because glyphs are paths, not drawText() calls.
//
#include <QPainterPath>
#include <QPainter>
#include <QString>
#include <QChar>
#include <QColor>
#include <QHash>
#include <QVector>
#include <functional>

namespace MathText {

// One harvested character: outline path (baseline at y=0, sized to kBaseEm)
// plus its horizontal advance and outline length (computed ONCE at harvest -
// QPainterPath::length() walks every element and is far too slow per frame).
struct Glyph {
    QPainterPath path;
    qreal        advance = 0;
    qreal        length = 0;
};

// How long a line of formula text takes to write itself on, in seconds. ONE
// value app-wide: the geometry walkthrough and the factorial estimate card both
// read it, so formula text draws at a single pace and tuning here tunes both.
// Drive progress from a wall clock against this, never a fixed step per timer
// tick - stepping per tick makes the speed depend on how often the timer fires.
constexpr qreal kDrawOnSeconds = 1.4;

// Em size the outlines are harvested at; layout scales from this. Large so
// downscaled curves stay crisp.
constexpr qreal kBaseEm = 200.0;

class GlyphLib {
public:
    static GlyphLib& instance();

    // Switch source font ("pattern that goes to other fonts"). Clears cache.
    void setFamily(const QString& family);
    QString family() const { return m_family; }

    const Glyph& glyph(QChar ch);

private:
    GlyphLib() = default;
    // Noto Sans Regular - the reference posters' annotation face, embedded via
    // resources/fonts/NotoSans-Regular.ttf. (CMU Serif, also bundled, is the
    // TeX/manim alternative for formula text; switch via setFamily.)
    QString m_family = QStringLiteral("Noto Sans");
    QHash<QChar, Glyph> m_cache;
};

// A glyph positioned by layout: its path is already translated/scaled into
// line-local coordinates (baseline at y=0, line starts at x=0).
//
// PERF: layout() transforms QPainterPaths, which allocates - cache the
// returned vector wherever the text is drawn every frame (the walkthrough
// caches per indicator/term) instead of re-laying-out per frame.
struct Placed {
    QPainterPath path;
    QChar        ch;
    int          group;   // caller-defined colour/animation group
    qreal        len = 0; // outline length at this scale (for draw-on tracing)
};

// Minimal math line builder:  Line().run("πr").sup("2")  ->  πr²
class Line {
public:
    // Normal baseline run.
    Line& run(const QString& s, int group = 0);
    // Superscript attached after the previous token (raised, ~0.6 scale).
    Line& sup(const QString& s, int group = 0);

    // Lay out at `em` pixels tall (glyph paths in line-local coords).
    QVector<Placed> layout(qreal em) const;
    qreal width(qreal em) const;

private:
    struct Tok { QString s; bool sup; int group; };
    QVector<Tok> m_toks;
};

// Per-group styling for draw().
struct Style {
    std::function<QColor(int group, QChar ch)> colorFor;  // required
    qreal  outlineWidth = 0.8;   // stroke width while tracing
    QColor halo = QColor(0, 0, 0, 170);  // legibility ring; alpha 0 disables
};

// Render placed glyphs with baseline origin at `pos`.
//   t = 1        -> static filled text (with halo)
//   t in [0,1)   -> staggered per-glyph draw-on: outline traces itself, then
//                   the fill fades in behind it.
void draw(QPainter& p, const QVector<Placed>& glyphs, const QPointF& pos,
          const Style& style, qreal t = 1.0);

// Pre-harvests the characters the walkthroughs use. Harvesting (outline +
// length) costs real time per character - call this once at startup (next to
// NaturalLanguage::warmUp) so the first formula click doesn't hitch.
void warmUp();

} // namespace MathText
