#include "MathText.h"
#include <QFont>
#include <QFontMetricsF>
#include <QPen>
#include <QTransform>
#include <algorithm>

namespace MathText {

// ---------------------------------------------------------------------------
// GlyphLib - harvests true vector outlines from the source font
// ---------------------------------------------------------------------------

GlyphLib& GlyphLib::instance() {
    static GlyphLib lib;
    return lib;
}

void GlyphLib::setFamily(const QString& family) {
    if (family == m_family) return;
    m_family = family;
    m_cache.clear();
}

const Glyph& GlyphLib::glyph(QChar ch) {
    auto it = m_cache.find(ch);
    if (it != m_cache.end()) return *it;

    QFont font(m_family);
    font.setPixelSize(int(kBaseEm));

    Glyph g;
    // addText() resolves the character through the font's outline data - the
    // same Béziers the reference posters were set with.
    g.path.addText(QPointF(0, 0), font, QString(ch));
    g.advance = QFontMetricsF(font).horizontalAdvance(ch);
    g.length = g.path.length();   // once, here - never per frame
    return *m_cache.insert(ch, g);
}

// ---------------------------------------------------------------------------
// Line - minimal math layout (runs + superscripts)
// ---------------------------------------------------------------------------

namespace {
    constexpr qreal kSupScale = 0.60;   // superscript size ratio
    constexpr qreal kSupRaise = 0.42;   // baseline raise, in em
}

Line& Line::run(const QString& s, int group) {
    m_toks.push_back({ s, false, group });
    return *this;
}

Line& Line::sup(const QString& s, int group) {
    m_toks.push_back({ s, true, group });
    return *this;
}

QVector<Placed> Line::layout(qreal em) const {
    QVector<Placed> out;
    GlyphLib& lib = GlyphLib::instance();

    // `em` is the one true size knob. Glyph scale, advance, superscript raise
    // and width() must all derive from the SAME scale, or spacing/centering
    // drift apart (an earlier size tweak doubled only this scale, which made
    // width() report half the real width and squashed superscript raises).
    const qreal scale = em / kBaseEm;
    qreal x = 0;

    for (const Tok& tok : m_toks) {
        const qreal s  = tok.sup ? scale * kSupScale : scale;
        const qreal dy = tok.sup ? -kSupRaise * em : 0.0;

        for (const QChar& ch : tok.s) {
            const Glyph& g = lib.glyph(ch);
            QTransform tr;
            tr.translate(x, dy);
            tr.scale(s, s);
            // Outline length scales linearly with the uniform glyph scale.
            out.push_back({ tr.map(g.path), ch, tok.group, g.length * s });
            x += g.advance * s;
        }
    }
    return out;
}

qreal Line::width(qreal em) const {
    GlyphLib& lib = GlyphLib::instance();
    const qreal scale = em / kBaseEm;
    qreal x = 0;
    for (const Tok& tok : m_toks) {
        const qreal s = tok.sup ? scale * kSupScale : scale;
        for (const QChar& ch : tok.s)
            x += lib.glyph(ch).advance * s;
    }
    return x;
}

// ---------------------------------------------------------------------------
// draw - staggered per-glyph outline trace + fill fade (the animation layer)
// ---------------------------------------------------------------------------

void draw(QPainter& p, const QVector<Placed>& glyphs, const QPointF& pos,
          const Style& style, qreal t) {
    if (glyphs.isEmpty()) return;

    p.save();
    p.translate(pos);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int n = glyphs.size();
    // Per-glyph stagger: glyph i animates in a window offset from its
    // neighbours, so the string writes itself left to right.
    const qreal stagger = (n > 1) ? qMin(0.08, 0.7 / n) : 0.0;

    for (int i = 0; i < n; ++i) {
        const Placed& pg = glyphs[i];
        if (pg.path.isEmpty()) continue;   // spaces

        qreal ti = 1.0;
        if (t < 1.0) {
            const qreal start = i * stagger;
            const qreal span  = 1.0 - (n - 1) * stagger;
            ti = std::clamp((t - start) / span, 0.0, 1.0);
        }
        if (ti <= 0.0) continue;

        const QColor col = style.colorFor ? style.colorFor(pg.group, pg.ch)
                                          : QColor(Qt::white);

        // Outline trace: first ~65% of the glyph's window.
        const qreal trace = std::clamp(ti / 0.65, 0.0, 1.0);
        // Fill fade: last ~45%, overlapping the trace.
        const qreal fill  = std::clamp((ti - 0.55) / 0.45, 0.0, 1.0);

        // Legibility halo behind the (partially) filled glyph.
        if (fill > 0.0 && style.halo.alpha() > 0) {
            QColor h = style.halo;
            h.setAlphaF(h.alphaF() * fill * col.alphaF());
            p.strokePath(pg.path, QPen(h, style.outlineWidth + 2.2,
                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }

        if (fill > 0.0) {
            QColor c = col;
            c.setAlphaF(c.alphaF() * fill);
            p.fillPath(pg.path, c);
        }

        if (trace < 1.0) {
            // Reveal the outline progressively: the SVG dash-offset trick.
            // Dash lengths are in pen-width units. Length was cached at
            // harvest - QPainterPath::length() is too slow to call per frame.
            const qreal len = pg.len;
            const qreal w   = style.outlineWidth;
            if (len > 0.0 && w > 0.0) {
                QPen pen(col, w, Qt::CustomDashLine, Qt::RoundCap, Qt::RoundJoin);
                const qreal L = len / w;
                pen.setDashPattern({ L, L });
                pen.setDashOffset(L * (1.0 - trace));
                p.strokePath(pg.path, pen);
            }
        }
        else if (fill < 1.0) {
            // Trace done but fill still fading: keep a solid outline so the
            // glyph never dips in contrast mid-transition.
            p.strokePath(pg.path, QPen(col, style.outlineWidth,
                Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        }
    }

    p.restore();
}

void warmUp() {
    GlyphLib& lib = GlyphLib::instance();
    // Letters, digits and punctuation used by the walkthrough annotations.
    static const char* kChars =
        "SurfaceAreaTm: =2(lw+h)0123456789.-\xCF\x80";
    for (const QChar& ch : QString::fromUtf8(kChars))
        lib.glyph(ch);
}

} // namespace MathText
