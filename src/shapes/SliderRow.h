#pragma once
#include <QWidget>
#include <QSlider>
#include <QLabel>
#include <QLineEdit>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QPainter>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QTimer>
#include <cmath>
#include <algorithm>
#include "../constants/Theme.h"

class SliderRow : public QWidget {
    Q_OBJECT
public:
    SliderRow(const QString& name, double init,
        double minV, double maxV, double step,
        QWidget* parent = nullptr)
        : QWidget(parent), m_name(name), m_min(minV), m_max(maxV), m_step(step)
    {
        setStyleSheet("background:transparent;");
        auto* row = new QHBoxLayout(this);
        row->setContentsMargins(0, 2, 0, 2);
        row->setSpacing(8);

        // Name painted via eventFilter on a blank spacer widget
        m_nameWidget = new QWidget(this);
        m_nameWidget->setFixedWidth(44);
        m_nameWidget->setFixedHeight(22);
        m_nameWidget->setStyleSheet("background:transparent;");
        m_nameWidget->installEventFilter(this);

        m_slider = new QSlider(Qt::Horizontal);
        m_slider->setRange(0, toTick(maxV));
        m_slider->setValue(toTick(init));
        m_slider->setStyleSheet(QString(R"(
            QSlider::groove:horizontal{height:3px;background:%1;border-radius:2px;}
            QSlider::handle:horizontal{background:%2;border:2px solid %2;
                width:14px;height:14px;margin:-6px 0;border-radius:7px;}
            QSlider::handle:horizontal:hover{background:%3;border-color:%3;
                width:16px;height:16px;margin:-7px 0;border-radius:8px;}
            QSlider::sub-page:horizontal{background:%2;border-radius:2px;}
        )").arg(Theme::DIM, Theme::ACCENT(), Theme::TEXT));

        // FIX / FEATURE: was a read-only QLabel — value could only ever be
        // set to whatever the slider's tick resolution (m_step) allowed.
        // Now an editable QLineEdit so users can type an exact value at
        // full precision, independent of the slider's step size.
        m_value = init;
        m_valEdit = new QLineEdit(formatValue(init));
        m_valEdit->setFont(Theme::monoFont(9, QFont::Bold));
        m_valEdit->setFixedWidth(62);
        m_valEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_valEdit->setFrame(false);
        m_valEdit->setCursor(Qt::IBeamCursor);
        // FIX / FEATURE: no longer bounded to [minV, maxV] — the slider's
        // own drag range still stops at its ends (that's inherent to
        // dragging a handle), but typed input can go beyond it. Only
        // restrict to "looks like a real number", not a specific range.
        auto* validator = new QDoubleValidator(m_valEdit);
        validator->setNotation(QDoubleValidator::StandardNotation);
        validator->setDecimals(6);
        m_valEdit->setValidator(validator);
        m_valEdit->setStyleSheet(QString(
            "QLineEdit{color:%1;background:transparent;border:none;"
            "border-bottom:1px solid transparent;padding:0;}"
            "QLineEdit:focus{border-bottom:1px solid %1;background:rgba(255,255,255,12);}"
        ).arg(Theme::ACCENT()));
        m_valEdit->installEventFilter(this); // select-all on focus, Esc to cancel

        row->addWidget(m_nameWidget);
        row->addWidget(m_slider, 1);
        row->addWidget(m_valEdit);

        connect(m_slider, &QSlider::valueChanged, this, &SliderRow::onSliderMoved);
        connect(m_slider, &QSlider::sliderPressed, this, [this]() {
            m_showPopup = true;
            m_popupVal = fromTick(m_slider->value());
            update();
            m_edgeTimer->start(); // begin watching for "held at max" during this drag
            });
        connect(m_slider, &QSlider::sliderReleased, this, [this]() {
            m_showPopup = false;
            m_edgeTimer->stop();
            update();
            });
        connect(m_valEdit, &QLineEdit::editingFinished, this, &SliderRow::onValueEdited);
        if (m_step <= 0.0) {
            double range = m_max - m_min;
            m_step = (range > 0.0) ? range / 100.0 : 1.0;
        }
        // FEATURE: "dynamic" edge growth. While the handle is held pinned
        // at its maximum tick, this timer periodically expands both m_max
        // and m_step by the same factor. Because they scale together, the
        // slider's own tick COUNT — (max-min)/step — never changes, so
        // the handle stays visually put at the right edge; only what that
        // edge *means* keeps growing. That's what gives the "held at the
        // wall but the value keeps climbing" feel instead of the drag
        // just doing nothing once it hits the end.
        m_edgeTimer = new QTimer(this);
        m_edgeTimer->setInterval(kEdgeTimerIntervalMs);
        connect(m_edgeTimer, &QTimer::timeout, this, &SliderRow::onEdgeHold);
    }

    // FIX: value() now returns the authoritative m_value (set directly by
    // typed input) rather than re-deriving from the slider's current
    // tick. Previously any typed precision would have been lost the
    // moment it got read back through fromTick(toTick(v)).
    double value() const { return m_value; }

    void setValue(double v) {
        // FIX / FEATURE: no longer clamped to [m_min, m_max]. A typed
        // value can now legitimately sit outside the slider's designed
        // range; m_value keeps it exactly. toTick(v) may compute a tick
        // outside the slider's own [0, toTick(maxV)] range, but
        // QAbstractSlider::setValue() clamps internally to the widget's
        // own min/max, so the handle just pins to whichever end is
        // closer — it can't crash or go out of bounds visually.
        m_value = v;

        m_slider->blockSignals(true);
        m_slider->setValue(toTick(v));
        m_slider->blockSignals(false);

        if (m_valEdit && !m_valEdit->hasFocus())
            m_valEdit->setText(formatValue(v));

        emit valueChanged(v);
    }

signals:
    void valueChanged(double);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (obj == m_nameWidget && ev->type() == QEvent::Paint) {
            QPainter p(m_nameWidget);
            p.setFont(Theme::monoFont(9));
            p.setPen(QColor(Theme::MUTED));
            p.drawText(m_nameWidget->rect(),
                Qt::AlignVCenter | Qt::AlignLeft,
                m_name + " =");
            return true;
        }
        if (obj == m_valEdit) {
            if (ev->type() == QEvent::FocusIn) {
                // Select all so typing immediately replaces the value,
                // instead of the user having to clear it manually first.
                QTimer::singleShot(0, m_valEdit, &QLineEdit::selectAll);
            }
            else if (ev->type() == QEvent::KeyPress) {
                auto* ke = static_cast<QKeyEvent*>(ev);
                if (ke->key() == Qt::Key_Escape) {
                    // Cancel the edit without applying it.
                    m_valEdit->setText(formatValue(m_value));
                    m_valEdit->clearFocus();
                    return true;
                }
            }
        }
        return QWidget::eventFilter(obj, ev);
    }

    void paintEvent(QPaintEvent*) override {
        if (!m_showPopup) return;
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        QString text = m_name + " = " + QString::number(m_popupVal, 'g', 6);
        QFont f = Theme::monoFont(9, QFont::Bold);
        p.setFont(f);
        QFontMetrics fm(f);
        int textW = fm.horizontalAdvance(text) + 16;
        int textH = 22;

        int sliderX = m_slider->x();
        int sliderW = m_slider->width();
        int range = m_slider->maximum() - m_slider->minimum();
        int thumbX = sliderX + (range > 0
            ? (int)((double)(m_slider->value() - m_slider->minimum()) / range * sliderW)
            : 0);
        int px = thumbX - textW / 2;
        px = std::max(0, std::min(px, width() - textW));
        int py = m_slider->y() - textH - 4;

        QRect popupRect(px, py, textW, textH);
        p.setBrush(QColor(Theme::SURFACE));
        p.setPen(QColor(Theme::ACCENT()));
        p.drawRoundedRect(popupRect, 6, 6);
        p.drawText(popupRect, Qt::AlignCenter, text);
    }

private slots:
    // Fires only for direct user drags of the slider now — programmatic
    // moves via setValue() blockSignals() around this.
    void onSliderMoved(int tick) {
        double val = fromTick(tick);
        m_value = val;
        if (m_valEdit && !m_valEdit->hasFocus())
            m_valEdit->setText(formatValue(val));
        if (m_showPopup) { m_popupVal = val; update(); }
        emit valueChanged(val);
    }

    void onValueEdited() {
        bool ok = false;
        double v = m_valEdit->text().toDouble(&ok);
        if (!ok) {
            // Malformed input somehow got past the validator (e.g. empty
            // field on focus-out) — revert to the last good value.
            m_valEdit->setText(formatValue(m_value));
            return;
        }
        setValue(v); // syncs slider position, emits valueChanged (unbounded)
    }

    void onEdgeHold() {
        if (!m_slider->isSliderDown()) { m_edgeTimer->stop(); return; }
        if (m_slider->value() != m_slider->maximum() && m_slider->value() != m_slider->minimum()) return; // not pinned at the edge
        if (m_slider->value() == m_slider->minimum()) {// Grow the range downward while keeping the tick count fixed.
// If the current range is zero, fall back to m_step.
            double range = std::max(m_max - m_min, m_step);

            // Increase the total range by the same factor, but extend the minimum.
            double newMin = m_max - range * kEdgeGrowthFactor;
            newMin = std::max(newMin, kAbsoluteFloor);

            if (newMin >= m_min) {
                m_edgeTimer->stop();
                return; // hit the safety floor
            }

            // Scale the step by the same factor.
            m_step = m_step * (m_max - newMin) / range;

            m_min = newMin;
            m_value = m_min; // handle remains pinned to the lower edge

            // Keep the slider handle pinned at the minimum tick.
            m_slider->blockSignals(true);
            m_slider->setValue(m_slider->minimum());
            m_slider->blockSignals(false);

            if (m_valEdit && !m_valEdit->hasFocus())
                m_valEdit->setText(formatValue(m_value));

            m_popupVal = m_value;
            update();

            emit valueChanged(m_value);

        }
        if (m_slider->value() == m_slider->maximum()) { // Grow both max and step by the same factor so tick count stays
            // fixed (see the comment where m_edgeTimer connects). Guard
            // against a zero-width starting range (m_max == m_min) by
            // falling back to m_step as the base "range" to grow from.
            double range = std::max(m_max - m_min, m_step);
            double newMax = m_min + range * kEdgeGrowthFactor;
            newMax = std::min(newMax, kAbsoluteCeiling);

            if (newMax <= m_max) { m_edgeTimer->stop(); return; } // hit the safety ceiling

            m_step = m_step * (newMax - m_min) / range; // scale step by the same factor
            m_max = newMax;
            m_value = m_max; // the pinned handle now represents this larger value

            // Tick count is unchanged by design, so the slider's own
            // maximum() doesn't need updating — just keep the handle pinned
            // at that same top tick after the rescale.
            m_slider->blockSignals(true);
            m_slider->setValue(m_slider->maximum());
            m_slider->blockSignals(false);

            if (m_valEdit && !m_valEdit->hasFocus())
                m_valEdit->setText(formatValue(m_value));

            m_popupVal = m_value;
            update();

            emit valueChanged(m_value);
        }
    }

private:
    int    toTick(double v)  const { return (int)std::round((v - m_min) / m_step); }
    double fromTick(int t)   const { return m_min + t * m_step; }
    static QString formatValue(double v) { return QString::number(v, 'g', 6); }

    static constexpr double kEdgeGrowthFactor = 1.5;   // range multiplier per tick of holding
    static constexpr int    kEdgeTimerIntervalMs = 120;  // how often we check/grow while held
    static constexpr double kAbsoluteCeiling = 1.0e100; // sane stopping point
    static constexpr double kAbsoluteFloor = -1e100;

    QString  m_name;
    double   m_min, m_max, m_step;
    double   m_value = 0.0;   // authoritative value; independent of slider tick quantization
    bool     m_showPopup = false;
    double   m_popupVal = 0.0;

    QWidget* m_nameWidget = nullptr;
    QSlider* m_slider = nullptr;
    QLineEdit* m_valEdit = nullptr;
    QTimer* m_edgeTimer = nullptr; // drives dynamic range growth while held at max
};