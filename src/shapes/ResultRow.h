#pragma once
#include <QWidget>
#include <QPainter>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <algorithm>
#include "../constants/Theme.h"
#include "../render/MathText.h"

class ResultRow : public QWidget {
    Q_OBJECT
public:
    ResultRow(const QString& key, const QString& formula, QWidget* parent = nullptr)
        : QWidget(parent), m_key(key), m_formula(formula)
    {
        setMinimumHeight(kLineH);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setStyleSheet("background:transparent;");
        // Needed for the formula hover-glow: we track the cursor to know when
        // it is over the formula text specifically (not just the row).
        setMouseTracking(true);

        // Drives the formula write-on: t ramps 0 -> 1 over ~0.7s while the
        // MathText glyphs trace themselves in (same effect as the walkthrough
        // readout in the 3D viewer).
        m_writeAnim.setInterval(16);
        connect(&m_writeAnim, &QTimer::timeout, this, [this]() {
            m_formulaT = std::min(1.0, m_formulaT + 16.0 / 700.0);
            if (m_formulaT >= 1.0) m_writeAnim.stop();
            update();
        });
    }

    void setValue(const QString& v) { m_currentValue = v; updateGeometry(); update(); }
    QString value() const { return m_currentValue; }
    QString key() const { return m_key; }

signals:
    // Left-click on the ROW toggles the formula; `visible` is the NEW state.
    // Hiding the formula stops any running walkthrough.
    void formulaToggled(const QString& key, bool visible);
    // Left-click ON the visible formula text itself. GeoCard forwards this so
    // the 3D viewer can play the formula walkthrough animation.
    void formulaClicked(const QString& key);

public:
    QString displayText() const {
        QString line = QString("%1  %2").arg(m_key, m_currentValue);
        if (m_formulaVisible && !m_formula.isEmpty())
            line += QString(" = %1").arg(m_formula);
        return line;
    }

    // FIX (primary): report the width this row actually needs to show its
    // key/value/formula on one line, unwrapped. The container (GeoCard /
    // GeoModeWidget's properties panel) queries this to size itself to
    // fit � that's the main defense against clipping now, rather than
    // wrapping text inside the row.
    QSize sizeHint() const override {
        return QSize(naturalWidth(), kLineH);
    }

    // FIX (fallback only): if the panel still isn't wide enough � e.g. it
    // hit its own max-width clamp, or the window itself is very narrow �
    // fall back to wrapping instead of hard-clipping. hasHeightForWidth()
    // lets the parent QVBoxLayout grant extra vertical space when this
    // kicks in.
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return computeHeight(w); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (m_hovered || m_copied)
            p.fillRect(rect(), QColor(Theme::HOVER));

        QFont f = Theme::monoFont(9);
        QFont fb = Theme::monoFont(9, QFont::Bold);

        // Key
        p.setFont(f);
        p.setPen(QColor(Theme::MUTED));
        p.drawText(QRect(4, 0, 144, kLineH),
            Qt::AlignVCenter | Qt::AlignLeft, m_key);

        if (m_copied) {
            p.setFont(fb);
            p.setPen(QColor(Theme::ACCENT()));
            p.drawText(QRect(152, 0, width() - 152, kLineH),
                Qt::AlignVCenter | Qt::AlignLeft, "Copied!");
            return;
        }

        p.setFont(fb);
        p.setPen(QColor(Theme::INFO));
        QFontMetrics fmBold(fb);
        int valW = fmBold.horizontalAdvance(m_currentValue);
        p.drawText(QRect(152, 0, valW + 4, kLineH),
            Qt::AlignVCenter | Qt::AlignLeft, m_currentValue);

        if (!m_formulaVisible || m_formula.isEmpty()) {
            m_formulaRect = QRect();
            if (m_hovered) {
                p.setFont(f);
                p.setPen(QColor(Theme::MUTED));
                p.drawText(QRect(152 + valW + 8, 0, width(), kLineH),
                    Qt::AlignVCenter | Qt::AlignLeft,
                    "\u2610 Right click to copy");
            }
            return;
        }

        // Formula rendered through MathText (CMU glyph outlines) so it writes
        // itself into view - same draw-on as the walkthrough readout. Hovering
        // it swaps the dark legibility halo for an accent one: the glow that
        // advertises "click me -> play the walkthrough".
        const QString text = QStringLiteral(" = ") + m_formula;
        const int inlineX = 152 + valW + 8;
        const int inlineAvailW = width() - inlineX - 6;

        MathText::Line line;
        line.run(text);

        // Shrink-to-fit instead of wrapping: if the panel's max-width clamp
        // still isn't enough, scale the formula down rather than breaking the
        // math across lines.
        qreal em = kFormulaEm;
        qreal lw = line.width(em);
        if (lw > inlineAvailW && lw > 0.0) {
            em = std::max(8.0, em * inlineAvailW / lw);
            lw = line.width(em);
        }
        m_formulaRect = QRect(inlineX, 0, int(lw) + 8, kLineH);

        MathText::Style st;
        const QColor base = m_formulaHovered ? QColor(Theme::ACCENT())
                                             : QColor(Theme::FTEXT);
        st.colorFor = [&base](int, QChar) { return base; };
        st.outlineWidth = 1.0;
        if (m_formulaHovered) {
            QColor glow(Theme::ACCENT());
            glow.setAlpha(70);
            st.halo = glow;          // accent halo = hover glow
            st.outlineWidth = 1.3;
        }

        MathText::draw(p, line.layout(em),
            QPointF(inlineX, kLineH - 6.0), st, m_formulaT);
    }

    void enterEvent(QEnterEvent*) override { m_hovered = true;  update(); }
    void leaveEvent(QEvent*)       override {
        m_hovered = false;
        m_formulaHovered = false;
        update();
    }
    void resizeEvent(QResizeEvent*) override { updateGeometry(); }

    void mouseMoveEvent(QMouseEvent* e) override {
        const bool over = m_formulaVisible && !m_formulaRect.isNull()
            && m_formulaRect.contains(e->pos());
        if (over != m_formulaHovered) {
            m_formulaHovered = over;
            update();
        }
        QWidget::mouseMoveEvent(e);
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::RightButton) {
            QApplication::clipboard()->setText(displayText());
            m_copied = true;
            m_hovered = false;
            update();
            QTimer::singleShot(1200, this, [this]() {
                m_copied = false;
                update();
                });
        }
        else if (e->button() == Qt::LeftButton) {
            // Click ON the visible formula text plays the walkthrough; a
            // click anywhere else on the row toggles the formula as before.
            if (m_formulaVisible && !m_formulaRect.isNull()
                && m_formulaRect.contains(e->pos())) {
                emit formulaClicked(m_key);
            }
            else {
                m_formulaVisible = !m_formulaVisible;
                m_formulaHovered = false;
                if (m_formulaVisible) {
                    // Write the formula into view rather than popping it in.
                    m_formulaT = 0.0;
                    m_writeAnim.start();
                }
                else {
                    m_writeAnim.stop();
                }
                updateGeometry();
                update();
                emit formulaToggled(m_key, m_formulaVisible);
            }
        }
        QWidget::mousePressEvent(e);
    }

private:
    static constexpr int   kLineH = 22;
    static constexpr int   kIndent = 16;
    static constexpr qreal kFormulaEm = 15.0;  // MathText em for the formula

    // Width needed to show key + value + (formula or hover hint) on one
    // line with no clipping and no wrap. The hover hint is always
    // factored in too so hovering never causes a resize/jump.
    int naturalWidth() const {
        QFontMetrics fmBold(Theme::monoFont(9, QFont::Bold));
        int valW = fmBold.horizontalAdvance(m_currentValue);
        int base = 152 + valW + 8;

        QFontMetrics fm(Theme::monoFont(9));
        int hintW = fm.horizontalAdvance("\u2610 Right click to copy");
        int w = base + hintW;

        if (m_formulaVisible && !m_formula.isEmpty()) {
            // Formula width from MathText (CMU) - matches what paintEvent
            // actually renders.
            MathText::Line line;
            line.run(QStringLiteral(" = ") + m_formula);
            w = std::max(w, base + int(line.width(kFormulaEm)));
        }
        return w + 8;
    }

    // Rows are always a single line now: an over-long formula shrinks to fit
    // (see paintEvent) instead of wrapping - math reads badly broken across
    // lines, and the panel already grows to naturalWidth() first.
    int computeHeight(int /*w*/) const { return kLineH; }

    QString m_key, m_formula, m_currentValue;
    bool    m_hovered = false;
    bool    m_copied = false;
    bool    m_formulaVisible = false;
    bool    m_formulaHovered = false;  // cursor is over the formula text
    QRect   m_formulaRect;             // where the formula was last painted
    qreal   m_formulaT = 1.0;          // write-on progress (1 = fully drawn)
    QTimer  m_writeAnim;               // ticks m_formulaT 0 -> 1
};