#include "SettingsPage.h"
#include "../Animations.h"
#include "../../constants/Theme.h"
#include "../../settings/SettingsDef.h"
#include <QScrollArea>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QSizePolicy>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QComboBox>
#include <QSlider>
#include <QLineEdit>
#include <QPushButton>
#include <QColorDialog>
#include <QCompleter>
#include <QFontDatabase>
#include <QEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QParallelAnimationGroup>


// -- Helpers -------------------------------------------------------------------

static QFont MF(int pt, int w = QFont::Normal) {
    QFont f(Theme::fontFamily());
    f.setPointSize(pt);
    f.setWeight(static_cast<QFont::Weight>(w));
    f.setStyleHint(QFont::Monospace);
    return f;
}

// Looks up a category's display label by ID — used for back button text
static QString categoryLabel(CategoryId id) {
    for (const CategoryDef& c : allCategories())
        if (c.id == id) return c.label;
    return "Back";
}

// Fades a widget in or out
static void fadeWidget(QWidget* w, bool in, int ms = 180) {
    auto* eff = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect());
    if (!eff) {
        eff = new QGraphicsOpacityEffect(w);
        w->setGraphicsEffect(eff);
    }
    auto* anim = new QPropertyAnimation(eff, "opacity", w);
    anim->setDuration(ms);
    anim->setStartValue(in ? 0.0 : 1.0);
    anim->setEndValue(in ? 1.0 : 0.0);
    anim->setEasingCurve(in ? QEasingCurve::OutCubic : QEasingCurve::InCubic);
    if (!in) QObject::connect(anim, &QPropertyAnimation::finished,
        w, [w]() { w->hide(); });
    w->show();
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// -- CategoryCard --------------------------------------------------------------
// Inline — small enough to live here rather than its own file

class CategoryCard : public QWidget {
    Q_OBJECT
        // Drives the hover transition — interpolated 0→1 over 150ms.
        // Used for border glow intensity, border inset ("expand" effect),
        // and abstract background vibrancy.
        Q_PROPERTY(qreal hoverProgress READ hoverProgress WRITE setHoverProgress)

public:
    qreal hoverProgress() const { return m_hoverProgress; }
    void setHoverProgress(qreal v) { m_hoverProgress = v; update(); }

    explicit CategoryCard(const CategoryDef& def, QWidget* parent = nullptr)
        : QWidget(parent), m_def(def)
    {
        
        setFixedSize(441, 320);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_StyledBackground, true);

        m_hoverAnim = new QPropertyAnimation(this, "hoverProgress", this);
        m_hoverAnim->setDuration(150);
        m_hoverAnim->setEasingCurve(QEasingCurve::OutCubic);

        // Geometry grow animation — makes the card visibly larger on hover
        // by overriding its layout-assigned rect, expanded by GROW_PX on
        // each side. raise() in enterEvent keeps it drawn above neighbors.
        m_growAnim = new QPropertyAnimation(this, "geometry", this);
        m_growAnim->setDuration(150);
        m_growAnim->setEasingCurve(QEasingCurve::OutCubic);

        // Map the logical icon name to a visible emoji glyph
        if (def.iconName == "brush")   m_emoji = "🖌️";
        else if (def.iconName == "monitor") m_emoji = "🖥️";
        else if (def.iconName == "gear")    m_emoji = "⚙️";
        else if (def.iconName == "server")  m_emoji = "🖧";

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(6);
        layout->addStretch(1);

        m_label = new QLabel(def.label);
        m_label->setFont(MF(13, QFont::Bold));
        m_label->setStyleSheet(QString(
            "color:%1; background:rgba(0,0,0,90); border-radius:6px; padding:2px 10px;"
        ).arg(def.accentColor));
        m_label->setAlignment(Qt::AlignCenter);

        m_desc = new QLabel(def.description);
        m_desc->setFont(MF(8));
        m_desc->setStyleSheet(QString(
            "color:%1; background:rgba(0,0,0,90); border-radius:6px; padding:1px 8px;"
        ).arg(Theme::TEXT));
        m_desc->setAlignment(Qt::AlignCenter);
        m_desc->setWordWrap(false);

        // Hint — short purpose statement, muted, below the description
        m_hint = new QLabel(def.hint);
        m_hint->setFont(MF(8));
        m_hint->setStyleSheet(QString(
            "color:%1; background:rgba(0,0,0,90); border-radius:6px; padding:1px 8px;"
        ).arg(Theme::MUTED));
        m_hint->setAlignment(Qt::AlignCenter);
        m_hint->setWordWrap(false);

        // Single shared backdrop behind label, description, and hint
        auto* textBox = new QWidget;
        textBox->setMinimumWidth(200);
        textBox->setStyleSheet("background:transparent; border-radius:8px;");
        auto* textBoxLayout = new QVBoxLayout(textBox);
        textBoxLayout->setContentsMargins(0, 0, 0, 0);
        textBoxLayout->setSpacing(6);
        textBoxLayout->addWidget(m_label);
        textBoxLayout->addWidget(m_desc);
        textBoxLayout->addWidget(m_hint);

        layout->addWidget(textBox, 0, Qt::AlignCenter);
        layout->addStretch(1);

        updateStyle(false);
    }

signals:
    void clicked(CategoryId id);

protected:
    void mousePressEvent(QMouseEvent*) override { emit clicked(m_def.id); }

    void enterEvent(QEnterEvent*) override {
        updateStyle(true);
        m_hoverAnim->stop();
        m_hoverAnim->setStartValue(m_hoverProgress);
        m_hoverAnim->setEndValue(1.0);
        m_hoverAnim->start();

        // Grow beyond the layout cell — raise() so it overlaps neighbors
        // on top instead of being clipped/pushed by them.
        raise();
        m_growAnim->stop();
        m_growAnim->setStartValue(geometry());
        m_growAnim->setEndValue(m_normalGeom.adjusted(-GROW_PX, -GROW_PX, GROW_PX, GROW_PX));
        m_growAnim->start();
    }
    void leaveEvent(QEvent*) override {
        updateStyle(false);
        m_hoverAnim->stop();
        m_hoverAnim->setStartValue(m_hoverProgress);
        m_hoverAnim->setEndValue(0.0);
        m_hoverAnim->start();

        m_growAnim->stop();
        m_growAnim->setStartValue(geometry());
        m_growAnim->setEndValue(m_normalGeom);
        m_growAnim->start();
    }

    void resizeEvent(QResizeEvent* e) override {
        QWidget::resizeEvent(e);
        // Only the layout's resize calls land here at normal (non-grown) size,
        // since the grow animation overrides geometry afterward. Track the
        // layout-assigned rect so we know what to grow from / shrink back to.
        if (!m_growAnim || m_growAnim->state() != QAbstractAnimation::Running)
            m_normalGeom = QRect(pos(), e->size());
    }

    void paintEvent(QPaintEvent* e) override {
        QWidget::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::SmoothPixmapTransform);

        // -- Emoji icon --------------------------------------------------------
        // Large, mostly-opaque glyph in the upper portion of the card, behind
        // the label/description pills. setOpacity (not pen alpha) is used
        // since color emoji are full-color bitmaps, not outline glyphs.
        if (!m_emoji.isEmpty()) {
            QFont emojiFont = font();
            emojiFont.setPixelSize(72);
            p.setFont(emojiFont);
            p.setOpacity(0.9);

            QRect emojiRect = rect().adjusted(0, 12, 0, -rect().height() / 2);
            p.drawText(emojiRect, Qt::AlignHCenter | Qt::AlignVCenter, m_emoji);
            p.setOpacity(1.0);
        }

        // -- Border-only glow + "expand" effect --------------------------------
        // The border inset shrinks toward the widget edge as hoverProgress
        // rises, making the card appear to grow within its allocated cell.
        // The glow layers brighten and spread further at the same time.
        const QColor accent(m_def.accentColor);
        p.setOpacity(1.0);
        p.setBrush(Qt::NoBrush);

        const qreal inset = 2.0 - 1.25 * m_hoverProgress;   // 2.0 → 0.75
        const QRectF base = QRectF(rect()).adjusted(inset, inset, -inset, -inset);

        const int   glowLayers = 3 + static_cast<int>(2 * m_hoverProgress + 0.5); // 3 → 5
        const qreal maxSpread = 3.0 + 3.0 * m_hoverProgress;                      // 3 → 6
        const qreal glowAlpha = 0.06 + 0.06 * m_hoverProgress;                    // 0.06 → 0.12

        for (int i = glowLayers; i >= 1; --i) {
            const qreal spread = (maxSpread / glowLayers) * i;
            QColor glow = accent;
            glow.setAlphaF(glowAlpha * (1.0 - (qreal)(i - 1) / glowLayers));
            p.setPen(QPen(glow, 1.0 + spread));
            p.drawRoundedRect(base.adjusted(-spread, -spread, spread, spread), 14, 14);
        }

        // Crisp edge on top — brightens slightly on hover via thicker stroke
        p.setPen(QPen(accent, 1.5 + 0.5 * m_hoverProgress));
        p.drawRoundedRect(base, 14, 14);
    }

private:
    void updateStyle(bool hovered) {
        m_hovered = hovered;
        // Background-only stylesheet — border is hand-painted in paintEvent
        // so we can layer a soft glow behind a crisp 1.5px edge.
        setStyleSheet(QString(
            "CategoryCard { background:%1; border-radius:14px; }"
        ).arg(hovered ? Theme::HOVER : Theme::SURFACE));
        update();
    }

    CategoryDef  m_def;
    QString      m_emoji;
    QLabel* m_label = nullptr;
    QLabel* m_desc = nullptr;
    QLabel* m_hint = nullptr;
    bool         m_hovered = false;
    qreal        m_hoverProgress = 0.0;
    QPropertyAnimation* m_hoverAnim = nullptr;

    // Geometry grow on hover
    static constexpr int GROW_PX = 10;
    QPropertyAnimation* m_growAnim = nullptr;
    QRect                m_normalGeom;
};

// -- SubcategoryCard -----------------------------------------------------------

class SubcategoryCard : public QWidget {
    Q_OBJECT
public:
    explicit SubcategoryCard(const SubcategoryDef& def,
        const QString& accentColor,
        QWidget* parent = nullptr)
        : QWidget(parent), m_def(def), m_accent(accentColor)
    {
        setFixedHeight(120);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(4);
        layout->addStretch(1);

        auto* label = new QLabel(def.label);
        label->setFont(MF(11, QFont::Bold));
        label->setStyleSheet(QString("color:%1; background:transparent;").arg(accentColor));
        label->setAlignment(Qt::AlignCenter);

        auto* desc = new QLabel(def.description);
        desc->setFont(MF(8));
        desc->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::MUTED));
        desc->setAlignment(Qt::AlignCenter);
        desc->setWordWrap(true);

        layout->addWidget(label);
        layout->addWidget(desc);
        layout->addStretch(1);

        updateStyle(false);
    }

signals:
    void clicked(SubcategoryId id);

protected:
    void mousePressEvent(QMouseEvent*) override { emit clicked(m_def.id); }
    void enterEvent(QEnterEvent*)      override { updateStyle(true); }
    void leaveEvent(QEvent*)           override { updateStyle(false); }

private:
    void updateStyle(bool hovered) {
        setStyleSheet(QString(
            "SubcategoryCard { background:%1; border:1px solid %2; border-radius:10px; }"
        ).arg(
            hovered ? Theme::HOVER : Theme::SURFACE,
            hovered ? m_accent : Theme::BORDER
        ));
    }

    SubcategoryDef m_def;
    QString        m_accent;
};

// -- SettingRow ----------------------------------------------------------------

class SettingRow : public QWidget {
    Q_OBJECT
public:
    explicit SettingRow(const SettingDef& def, QWidget* parent = nullptr)
        : QWidget(parent), m_def(def)
    {
        setFixedHeight(48);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 0, 16, 0);
        layout->setSpacing(12);

        // Label — uses Basic or Advanced depending on visibility level
        m_label = new QLabel;
        m_label->setFont(MF(9));
        m_label->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::TEXT));
        layout->addWidget(m_label, 1);

        // Description — always visible in Basic, hover-only in Advanced/Developer
        m_desc = new QLabel;
        m_desc->setFont(MF(8));
        m_desc->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::MUTED));
        layout->addWidget(m_desc, 1);

        // Control area — built by buildControl() based on def.control
        m_controlContainer = new QWidget;
        auto* ctrlLayout = new QHBoxLayout(m_controlContainer);
        ctrlLayout->setContentsMargins(0, 0, 0, 0);
        ctrlLayout->setSpacing(6);
        ctrlLayout->setAlignment(Qt::AlignRight);
        buildControl(ctrlLayout);
        layout->addWidget(m_controlContainer);

        // Pending indicator — small dot + label shown when this key has a
        // staged/deferred change waiting. Sits below the control.
        m_pendingDot = new QLabel;
        m_pendingDot->setFont(MF(7));
        m_pendingDot->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::ACCENT()));
        m_pendingDot->setAlignment(Qt::AlignRight);
        m_pendingDot->hide();
        layout->addWidget(m_pendingDot);

        refreshForLevel(Settings::instance().visibilityLevel());

        connect(&Settings::instance(), &Settings::visibilityLevelChanged,
            this, &SettingRow::refreshForLevel);
        connect(&Settings::instance(), &Settings::pendingChanged,
            this, &SettingRow::refreshPendingIndicator);
        connect(&Settings::instance(), &Settings::pendingApplied,
            this, &SettingRow::onPendingApplied);
        connect(&Settings::instance(), &Settings::settingsReset,
            this, &SettingRow::refreshControlValue);

        refreshPendingIndicator();

        setStyleSheet("SettingRow { background:transparent; border-bottom:1px solid "
            + Theme::BORDER + "; }");
    }

private slots:
    void refreshForLevel(VisibilityLevel level) {
        const bool basic = (level == VisibilityLevel::Basic);
        m_label->setText(basic ? m_def.labelBasic : m_def.labelAdvanced);
        m_desc->setText(basic ? m_def.descBasic : "");
        m_desc->setVisible(basic);
    }

    // Re-checks the pending queue for an entry matching this row's key and
    // shows/hides the pending dot with the right wording for the apply mode.
    void refreshPendingIndicator() {
        const PendingChange* mine = nullptr;
        for (const PendingChange& c : Settings::instance().pendingChanges()) {
            if (c.key == m_def.key) { mine = &c; break; }
        }
        if (!mine) {
            m_pendingDot->hide();
            return;
        }
        m_pendingDot->setText(
            m_def.applyMode == ApplyMode::Deferred
            ? "● will apply after current calculation"
            : "● will apply when idle");
        m_pendingDot->show();
    }

    // When this row's key is among the ones just applied, refresh the
    // displayed control value to match and hide the pending dot.
    void onPendingApplied(const QList<PendingChange>& applied) {
        for (const PendingChange& c : applied) {
            if (c.key == m_def.key) { refreshControlValue(); break; }
        }
        refreshPendingIndicator();
    }

protected:
    void enterEvent(QEnterEvent*) override {
        setStyleSheet("SettingRow { background:" + Theme::HOVER +
            "; border-bottom:1px solid " + Theme::BORDER + "; }");
        // Show description on hover for Advanced/Developer
        if (Settings::instance().visibilityLevel() != VisibilityLevel::Basic) {
            m_desc->setText(m_def.descAdvanced);
            m_desc->show();
        }
    }
    void leaveEvent(QEvent*) override {
        setStyleSheet("SettingRow { background:transparent; border-bottom:1px solid "
            + Theme::BORDER + "; }");
        if (Settings::instance().visibilityLevel() != VisibilityLevel::Basic) {
            m_desc->hide();
        }
    }

private:
    // -- Control construction ---------------------------------------------------
    // Dispatches on def.control and builds the appropriate widget(s) into
    // m_ctrlLayout. Each branch wires its own change → Settings::set(key, ...).
    void buildControl(QHBoxLayout* ctrlLayout) {
        switch (m_def.control) {
        case ControlType::Toggle:      buildToggle(ctrlLayout);      break;
        case ControlType::Dropdown:    buildDropdown(ctrlLayout);    break;
        case ControlType::Slider:      buildSlider(ctrlLayout);      break;
        case ControlType::ColorPicker: buildColorPicker(ctrlLayout); break;
        case ControlType::FontInput:   buildFontInput(ctrlLayout);   break;
        case ControlType::TextInput:   buildTextInput(ctrlLayout);   break;
        case ControlType::Action:      buildAction(ctrlLayout);      break;
        }
    }

    // -- Toggle — red/green push button ---------------------------------------
    void buildToggle(QHBoxLayout* ctrlLayout) {
        auto* btn = new QPushButton;
        btn->setFixedSize(72, 26);
        btn->setCursor(Qt::PointingHandCursor);

        auto refresh = [this, btn]() {
            const bool on = Settings::instance().get(m_def.key).toBool();
            btn->setText(on ? "On" : "Off");
            btn->setStyleSheet(QString(
                "QPushButton { background:%1; border:none; border-radius:6px;"
                "color:%2; font-weight:bold; }"
            ).arg(on ? Theme::ACCENT() : Theme::ERROR, "#000"));
            };
        refresh();

        connect(btn, &QPushButton::clicked, this, [this, refresh]() {
            const bool current = Settings::instance().get(m_def.key).toBool();
            Settings::instance().set(m_def.key, !current);
            refresh();
            });

        m_refreshFn = refresh;
        ctrlLayout->addWidget(btn);
    }

    // -- Dropdown — styled QComboBox from def.options -------------------------
    void buildDropdown(QHBoxLayout* ctrlLayout) {
        auto* combo = new QComboBox;
        combo->addItems(m_def.options);
        combo->setFont(MF(8));
        combo->setFixedWidth(140);
        combo->setStyleSheet(QString(
            "QComboBox { background:%1; border:1px solid %2; border-radius:6px;"
            "color:%3; padding:2px 8px; }"
            "QComboBox:hover { border-color:%4; }"
            "QComboBox QAbstractItemView { background:%1; color:%3;"
            "selection-background-color:%4; border:1px solid %2; }"
        ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT, Theme::ACCENT()));

        auto refresh = [this, combo]() {
            const QString current = Settings::instance().get(m_def.key).toString();
            int idx = combo->findText(current);
            combo->blockSignals(true);
            combo->setCurrentIndex(idx >= 0 ? idx : 0);
            combo->blockSignals(false);
            };
        refresh();

        connect(combo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
            Settings::instance().set(m_def.key, text);
            });

        m_refreshFn = refresh;
        ctrlLayout->addWidget(combo);
    }

    // -- Slider — adaptive-step slider + click-to-edit value label -----------
    void buildSlider(QHBoxLayout* ctrlLayout) {
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setFixedWidth(110);
        slider->setRange(0, SLIDER_STEPS);
        slider->setCursor(Qt::PointingHandCursor);
        slider->setStyleSheet(QString(
            "QSlider::groove:horizontal { background:%1; height:4px; border-radius:2px; }"
            "QSlider::handle:horizontal { background:%2; width:12px; margin:-5px 0;"
            "border-radius:6px; }"
        ).arg(Theme::BORDER, Theme::ACCENT()));

        auto* valueLabel = new QLabel;
        valueLabel->setFont(MF(9, QFont::Bold));
        valueLabel->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::ACCENT()));
        valueLabel->setFixedWidth(56);
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valueLabel->setCursor(Qt::IBeamCursor);

        const double minV = m_def.range.min, maxV = m_def.range.max;

        auto sliderPosFor = [minV, maxV](double v) -> int {
            if (maxV <= minV) return 0;
            return qBound(0, int(((v - minV) / (maxV - minV)) * SLIDER_STEPS + 0.5), SLIDER_STEPS);
            };
        auto valueForSliderPos = [minV, maxV](int pos) -> double {
            return minV + (double(pos) / SLIDER_STEPS) * (maxV - minV);
            };
        auto formatValue = [this](double v) -> QString {
            QString s = (v == int(v)) ? QString::number(int(v)) : QString::number(v, 'g', 4);
            return m_def.unit.isEmpty() ? s : s + " " + m_def.unit;
            };

        auto refresh = [this, slider, valueLabel, sliderPosFor, formatValue]() {
            const double v = Settings::instance().get(m_def.key).toDouble();
            slider->blockSignals(true);
            slider->setValue(sliderPosFor(v));
            slider->blockSignals(false);
            valueLabel->setText(formatValue(v));
            };
        refresh();

        connect(slider, &QSlider::valueChanged, this, [this, valueForSliderPos, formatValue, valueLabel](int pos) {
            const double v = valueForSliderPos(pos);
            const double rounded = (m_def.range.step >= 1.0) ? qRound(v) : v;
            valueLabel->setText(formatValue(rounded));
            Settings::instance().set(m_def.key, rounded);
            });

        // Click-to-edit: turn the value label into a QLineEdit on click,
        // commit on Enter or focus-out, clamped to range.
        valueLabel->installEventFilter(this);
        m_editTarget = valueLabel;
        m_editSlider = slider;
        m_editSliderPosFor = sliderPosFor;

        m_refreshFn = refresh;
        ctrlLayout->addWidget(slider);
        ctrlLayout->addWidget(valueLabel);
    }

    // -- ColorPicker — swatch + multi-format text input + picker button ------
    void buildColorPicker(QHBoxLayout* ctrlLayout) {
        auto* swatch = new QLabel;
        swatch->setFixedSize(20, 20);

        auto* edit = new QLineEdit;
        edit->setFixedWidth(90);
        edit->setFont(MF(8));
        edit->setStyleSheet(QString(
            "QLineEdit { background:%1; border:1px solid %2; border-radius:4px;"
            "color:%3; padding:2px 6px; }"
        ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT));

        auto* pickBtn = new QPushButton("⬚");
        pickBtn->setFixedSize(24, 24);
        pickBtn->setCursor(Qt::PointingHandCursor);
        pickBtn->setStyleSheet(QString(
            "QPushButton { background:%1; border:1px solid %2; border-radius:4px; color:%3; }"
            "QPushButton:hover { border-color:%4; }"
        ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT, Theme::ACCENT()));

        auto applyColor = [this, swatch](const QColor& c) {
            swatch->setStyleSheet(QString(
                "background:%1; border:1px solid %2; border-radius:4px;"
            ).arg(c.name(), Theme::BORDER));
            Settings::instance().set(m_def.key, c.name());
            };

        auto refresh = [this, swatch, edit]() {
            const QColor c(Settings::instance().get(m_def.key).toString());
            swatch->setStyleSheet(QString(
                "background:%1; border:1px solid %2; border-radius:4px;"
            ).arg(c.name(), Theme::BORDER));
            edit->blockSignals(true);
            edit->setText(c.name());
            edit->blockSignals(false);
            };
        refresh();

        connect(edit, &QLineEdit::editingFinished, this, [this, edit, applyColor]() {
            QColor parsed = parseColorInput(edit->text());
            if (parsed.isValid()) applyColor(parsed);
            else edit->setText(QColor(Settings::instance().get(m_def.key).toString()).name());
            });

        connect(pickBtn, &QPushButton::clicked, this, [this, applyColor, edit]() {
            QColor start(Settings::instance().get(m_def.key).toString());
            QColor chosen = QColorDialog::getColor(start, this, "Choose color");
            if (chosen.isValid()) { applyColor(chosen); edit->setText(chosen.name()); }
            });

        m_refreshFn = refresh;
        ctrlLayout->addWidget(swatch);
        ctrlLayout->addWidget(edit);
        ctrlLayout->addWidget(pickBtn);
    }

    // -- FontInput — text field with QCompleter over installed font families -
    void buildFontInput(QHBoxLayout* ctrlLayout) {
        auto* edit = new QLineEdit;
        edit->setFixedWidth(150);
        edit->setFont(MF(8));
        edit->setStyleSheet(QString(
            "QLineEdit { background:%1; border:1px solid %2; border-radius:4px;"
            "color:%3; padding:2px 6px; }"
        ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT));

        auto* completer = new QCompleter(QFontDatabase::families(), edit);
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setCompletionMode(QCompleter::PopupCompletion);
        edit->setCompleter(completer);

        auto refresh = [this, edit]() {
            edit->blockSignals(true);
            edit->setText(Settings::instance().get(m_def.key).toString());
            edit->blockSignals(false);
            };
        refresh();

        connect(edit, &QLineEdit::editingFinished, this, [this, edit]() {
            const QString name = edit->text().trimmed();
            if (QFontDatabase::families().contains(name)) {
                Settings::instance().set(m_def.key, name);
            }
            else {
                // Invalid font — revert to last valid value
                edit->setText(Settings::instance().get(m_def.key).toString());
            }
            });

        m_refreshFn = refresh;
        ctrlLayout->addWidget(edit);
    }

    // -- TextInput — plain text field -----------------------------------------
    void buildTextInput(QHBoxLayout* ctrlLayout) {
        auto* edit = new QLineEdit;
        edit->setFixedWidth(150);
        edit->setFont(MF(8));
        edit->setStyleSheet(QString(
            "QLineEdit { background:%1; border:1px solid %2; border-radius:4px;"
            "color:%3; padding:2px 6px; }"
        ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT));

        auto refresh = [this, edit]() {
            edit->blockSignals(true);
            edit->setText(Settings::instance().get(m_def.key).toString());
            edit->blockSignals(false);
            };
        refresh();

        connect(edit, &QLineEdit::editingFinished, this, [this, edit]() {
            Settings::instance().set(m_def.key, edit->text());
            });

        m_refreshFn = refresh;
        ctrlLayout->addWidget(edit);
    }

    // -- Action — one-shot button, no persisted state -------------------------
    void buildAction(QHBoxLayout* ctrlLayout) {
        auto* btn = new QPushButton(m_def.labelBasic);
        btn->setFont(MF(8));
        btn->setFixedHeight(26);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(QString(
            "QPushButton { background:none; border:1px solid %1; color:%2;"
            "border-radius:6px; padding:2px 12px; }"
            "QPushButton:hover { border-color:%3; color:%3; }"
        ).arg(Theme::BORDER, Theme::MUTED, Theme::ACCENT()));

        connect(btn, &QPushButton::clicked, this, [this]() {
            emit actionTriggered(m_def.key);
            });

        ctrlLayout->addWidget(btn);
    }

    // Re-reads the current value from Settings and updates the control's
    // displayed state. Called after a pending change applies or settings reset.
    void refreshControlValue() {
        if (m_refreshFn) m_refreshFn();
    }

    // Parses #RRGGBB, rgb(r,g,b), "r,g,b", or packed rrggbb into a QColor.
    static QColor parseColorInput(const QString& raw) {
        const QString s = raw.trimmed();
        if (s.startsWith('#')) {
            QColor c(s);
            return c;
        }
        if (s.startsWith("rgb", Qt::CaseInsensitive)) {
            QString inner = s.mid(s.indexOf('(') + 1);
            inner.chop(1);
            QStringList parts = inner.split(',');
            if (parts.size() == 3)
                return QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
        }
        if (s.contains(',')) {
            QStringList parts = s.split(',');
            if (parts.size() == 3)
                return QColor(parts[0].trimmed().toInt(), parts[1].trimmed().toInt(), parts[2].trimmed().toInt());
        }
        if (s.length() == 6 && std::all_of(s.begin(), s.end(), [](QChar c) { return c.isLetterOrNumber(); })) {
            return QColor("#" + s);
        }
        return QColor(); // invalid
    }

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (obj == m_editTarget && ev->type() == QEvent::MouseButtonPress) {
            startSliderEdit();
            return true;
        }
        return QWidget::eventFilter(obj, ev);
    }

private:
    // Swaps the slider's value QLabel for a temporary QLineEdit so the user
    // can type an exact value directly. Commits and clamps on Enter/focus-out.
    void startSliderEdit() {
        if (!m_editTarget || m_editingSlider) return;
        m_editingSlider = true;

        auto* edit = new QLineEdit(m_editTarget->text(), m_editTarget->parentWidget());
        edit->setFont(m_editTarget->font());
        edit->setFixedSize(m_editTarget->size());
        edit->setAlignment(Qt::AlignRight);
        edit->setStyleSheet(QString(
            "QLineEdit { background:%1; border:1px solid %2; color:%3; }"
        ).arg(Theme::SURFACE, Theme::ACCENT(), Theme::ACCENT()));

        auto* parentLayout = qobject_cast<QHBoxLayout*>(m_editTarget->parentWidget()->layout());
        int idx = parentLayout->indexOf(m_editTarget);
        m_editTarget->hide();
        parentLayout->insertWidget(idx, edit);
        edit->setFocus();
        edit->selectAll();

        auto commit = [this, edit]() {
            bool ok = false;
            double v = edit->text().trimmed().split(' ').first().toDouble(&ok);
            if (ok) {
                v = qBound(m_def.range.min, v, m_def.range.max);
                Settings::instance().set(m_def.key, v);
                if (m_editSlider) m_editSlider->setValue(m_editSliderPosFor(v));
            }
            if (m_refreshFn) m_refreshFn();
            edit->deleteLater();
            m_editTarget->show();
            m_editingSlider = false;
            };
        connect(edit, &QLineEdit::editingFinished, this, commit);
    }

    static constexpr int SLIDER_STEPS = 1000; // internal resolution, independent of range

    SettingDef   m_def;
    QLabel* m_label = nullptr;
    QLabel* m_desc = nullptr;
    QLabel* m_pendingDot = nullptr;
    QWidget* m_controlContainer = nullptr;

    std::function<void()> m_refreshFn;

    // Slider click-to-edit support
    QLabel* m_editTarget = nullptr;
    QSlider* m_editSlider = nullptr;
    std::function<int(double)> m_editSliderPosFor;
    bool     m_editingSlider = false;

signals:
    // Bubbled up so SettingsPage can invoke the right action callback
    // (file dialogs, confirmation dialogs) without Settings depending on Qt UI.
    void actionTriggered(QString key);
};

// -- SearchResultRow -----------------------------------------------------------
// One row in the search results list. Shows a breadcrumb (Category → Sub)
// in muted/accent colors, the setting's label, and its description.
// Clicking emits clicked(SettingDef) so SettingsPage can navigate to it.

class SearchResultRow : public QWidget {
    Q_OBJECT
public:
    explicit SearchResultRow(const SettingDef& def, QWidget* parent = nullptr)
        : QWidget(parent), m_def(def)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(56);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 6, 16, 6);
        layout->setSpacing(2);

        // Breadcrumb — "Appearance → Typography" in category accent + muted
        QString accent = Theme::ACCENT();
        QString catLabel, subLabel;
        for (const CategoryDef& c : allCategories()) {
            if (c.id == def.category) { accent = c.accentColor; catLabel = c.label; break; }
        }
        for (const SubcategoryDef& s : allSubcategories()) {
            if (s.id == def.subcategory) { subLabel = s.label; break; }
        }

        auto* breadcrumb = new QLabel(
            QString("<span style='color:%1;'>%2</span>"
                "<span style='color:%3;'> → </span>"
                "<span style='color:%4;'>%5</span>")
            .arg(accent, catLabel, Theme::MUTED, Theme::MUTED, subLabel));
        breadcrumb->setFont(MF(8));
        breadcrumb->setStyleSheet("background:transparent;");
        layout->addWidget(breadcrumb);

        // Setting label — Basic or Advanced depending on visibility level
        const bool basic = (Settings::instance().visibilityLevel() == VisibilityLevel::Basic);
        m_label = new QLabel(basic ? def.labelBasic : def.labelAdvanced);
        m_label->setFont(MF(10, QFont::Bold));
        m_label->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::TEXT));
        layout->addWidget(m_label);

        // Description — Basic description, always shown in search results
        auto* desc = new QLabel(def.descBasic);
        desc->setFont(MF(8));
        desc->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::MUTED));
        layout->addWidget(desc);

        setStyleSheet("SearchResultRow { background:transparent; border-bottom:1px solid "
            + Theme::BORDER + "; }");
    }

signals:
    void clicked(SettingDef def);

protected:
    void mousePressEvent(QMouseEvent*) override { emit clicked(m_def); }

    void enterEvent(QEnterEvent*) override {
        setStyleSheet("SearchResultRow { background:" + Theme::HOVER +
            "; border-bottom:1px solid " + Theme::BORDER + "; }");
    }
    void leaveEvent(QEvent*) override {
        setStyleSheet("SearchResultRow { background:transparent; border-bottom:1px solid "
            + Theme::BORDER + "; }");
    }

private:
    SettingDef m_def;
    QLabel* m_label = nullptr;
};

// -- VisibilityControl ---------------------------------------------------------

VisibilityControl::VisibilityControl(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* row = new QHBoxLayout;
    row->setSpacing(6);

    m_levelBtn = new QPushButton("Basic ▾");
    m_levelBtn->setFont(MF(9));
    m_levelBtn->setCursor(Qt::PointingHandCursor);
    m_levelBtn->setFixedHeight(28);
    row->addWidget(m_levelBtn);

    m_helpBtn = new QPushButton("?");
    m_helpBtn->setFont(MF(9));
    m_helpBtn->setFixedSize(20, 20);
    m_helpBtn->setCursor(Qt::PointingHandCursor);
    m_helpBtn->hide(); // only shown in Passive state
    row->addWidget(m_helpBtn);

    layout->addLayout(row);

    m_hintLabel = new QLabel(
        "This setting changes how comfortable you are with the settings page.\n"
        "Modify to show or hide more settings.");
    m_hintLabel->setFont(MF(8));
    m_hintLabel->setStyleSheet(
        QString("color:%1; background:transparent;").arg(Theme::MUTED));
    m_hintLabel->setWordWrap(true);
    m_hintLabel->hide();
    layout->addWidget(m_hintLabel);

    // Connect level button
    connect(m_levelBtn, &QPushButton::clicked, this, [this]() {
        // Cycle through levels for now — dropdown popup in follow-up
        VisibilityLevel current = Settings::instance().visibilityLevel();
        VisibilityLevel next;
        switch (current) {
        case VisibilityLevel::Basic:    next = VisibilityLevel::Advanced;  break;
        case VisibilityLevel::Advanced: next = VisibilityLevel::Developer; break;
        default:                        next = VisibilityLevel::Basic;     break;
        }
        Settings::instance().setVisibilityLevel(next);
        const char* labels[] = { "Basic ▾", "Advanced ▾", "Developer ▾" };
        m_levelBtn->setText(labels[static_cast<int>(next)]);
        emit levelChanged(next);
        });

    connect(m_helpBtn, &QPushButton::clicked, this, [this]() {
        m_hintLabel->isVisible() ? hideHintText() : showHintText();
        });

    // Observe hint state from Settings
    connect(&Settings::instance(), &Settings::hintStateChanged,
        this, &VisibilityControl::onHintStateChanged);

    // Enter the correct state on construction
    onHintStateChanged(Settings::instance().hintState());
}

void VisibilityControl::onHintStateChanged(HintState state) {
    switch (state) {
    case HintState::Active:  enterActiveState();  break;
    case HintState::Passive: enterPassiveState(); break;
    case HintState::Dormant: enterDormantState(); break;
    }
}

void VisibilityControl::enterActiveState() {
    m_levelBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%1;"
        "border-radius:6px; padding:2px 10px; }"
        "QPushButton:hover { border-color:%2; color:%2; }"
    ).arg(Theme::ACCENT(), Theme::ACCENT_DIM));
    m_helpBtn->hide();
    showHintText();
}

void VisibilityControl::enterPassiveState() {
    m_levelBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%2;"
        "border-radius:6px; padding:2px 10px; }"
        "QPushButton:hover { border-color:%3; color:%3; }"
    ).arg(Theme::BORDER, Theme::TEXT, Theme::ACCENT()));
    m_helpBtn->show();
    hideHintText();
}

void VisibilityControl::enterDormantState() {
    // Bare label — no border, no glow, no ? button
    m_levelBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:none; color:%1;"
        "padding:2px 10px; }"
        "QPushButton:hover { color:%2; }"
    ).arg(Theme::MUTED, Theme::TEXT));
    m_helpBtn->hide();
    hideHintText();
}

void VisibilityControl::showHintText() {
    fadeWidget(m_hintLabel, true, 200);
}

void VisibilityControl::hideHintText() {
    if (m_hintLabel->isVisible())
        fadeWidget(m_hintLabel, false, 200);
}

// -- PendingQueueFooter --------------------------------------------------------

PendingQueueFooter::PendingQueueFooter(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 8, 16, 8);
    layout->setSpacing(4);

    m_queueList = new QWidget;
    m_listLayout = new QVBoxLayout(m_queueList);
    m_listLayout->setContentsMargins(0, 0, 0, 0);
    m_listLayout->setSpacing(2);
    layout->addWidget(m_queueList);

    m_countLabel = new QLabel("0 pending changes");
    m_countLabel->setFont(MF(9));
    m_countLabel->setStyleSheet(
        QString("color:%1; background:transparent;").arg(Theme::MUTED));
    m_countLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_countLabel);

    connect(&Settings::instance(), &Settings::pendingChanged,
        this, &PendingQueueFooter::onPendingChanged);

    onPendingChanged();
}

void PendingQueueFooter::onPendingChanged() {
    rebuild();
}

void PendingQueueFooter::rebuild() {
    // Clear existing entries
    QLayoutItem* item;
    while ((item = m_listLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    const QList<PendingChange>& pending = Settings::instance().pendingChanges();

    for (const PendingChange& change : pending) {
        auto* row = new QWidget;
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(8);

        auto* labelLbl = new QLabel(change.label);
        labelLbl->setFont(MF(8));
        labelLbl->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::MUTED));

        auto* arrow = new QLabel("→");
        arrow->setFont(MF(8));
        arrow->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::MUTED));

        // Append unit suffix if the setting has one (e.g. "13 px")
        const SettingDef* def = findSetting(change.key);
        const QString unit = def ? def->unit : QString();

        auto* oldLbl = new QLabel(change.oldValue.toString() + (unit.isEmpty() ? "" : " " + unit));
        oldLbl->setFont(MF(8));
        oldLbl->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::TEXT));

        auto* newLbl = new QLabel(change.newValue.toString() + (unit.isEmpty() ? "" : " " + unit));
        newLbl->setFont(MF(8, QFont::Bold));
        newLbl->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::ACCENT()));

        rl->addWidget(labelLbl, 1);
        rl->addWidget(oldLbl);
        rl->addWidget(arrow);
        rl->addWidget(newLbl);

        m_listLayout->addWidget(row);
    }

    const int count = pending.size();
    m_countLabel->setText(
        count == 0 ? "0 pending changes"
        : QString::number(count) + (count == 1 ? " pending change"
            : " pending changes"));

    // Highlight counter when changes exist
    m_countLabel->setStyleSheet(QString("color:%1; background:transparent;").arg(
        count > 0 ? Theme::ACCENT() : Theme::MUTED));

    m_queueList->setVisible(count > 0);
}

void PendingQueueFooter::playApplySequence(std::function<void()> onComplete) {
    if (!Settings::instance().hasPendingChanges()) {
        if (onComplete) onComplete();
        return;
    }

    // Decide whether to play the full animation. For Once, this call also
    // performs the auto-downgrade to Never as a side effect.
    const bool wasOnce = (Settings::instance().animationMode() == AnimationMode::Once);
    const bool playAnim = Settings::instance().shouldPlayApplyAnimation();

    // Full theatrical apply sequence (queue expand → header float → arrow
    // step-through → status labels) lives in SettingsAnimations, a future
    // module. For now both paths apply immediately; the branch exists so
    // the animation can be slotted in without touching this call site.
    if (playAnim) {
        Settings::instance().applyPending(true);

        // This was the first-ever Once playback — show the one-shot message
        // pointing the user to where to re-enable it.
        if (wasOnce && !Settings::instance().hasSeenPostAnimationHint()) {
            Settings::instance().markPostAnimationHintSeen();
            emit applySequenceFinishedFirstTime();
        }
    }
    else {
        Settings::instance().applyPending(true);
    }

    if (onComplete) onComplete();
}
// -- ApplyAnimationOverlay -------------------------------------------------
// Full-page overlay that plays the "applying changes" sequence:
//   1. pending entries fly from the bottom to a row near the top
//   2. the counter label grows into a centered page heading
//   3. an arrow steps through each change with a live status subtitle
//   4. (first time only) shows the "animation will be disabled" message
//      with a Continue button; otherwise auto-dismisses.
class ApplyAnimationOverlay : public QWidget {
    Q_OBJECT
public:
    explicit ApplyAnimationOverlay(QWidget* parent)
        : QWidget(parent)
    {
        setStyleSheet(QString("background:%1;").arg(Theme::BG));
        setGeometry(parent->rect());
    }

    // changes         — snapshot of what was just applied (already committed
    //                   to Settings before this plays — purely visual reassurance)
    // showDisableHint — true the first time "Once" auto-downgrades to "Never"
    void play(const QList<PendingChange>& changes, bool showDisableHint,
        std::function<void()> onComplete)
    {
        m_changes = changes;
        m_onComplete = onComplete;
        m_showDisableHint = showDisableHint;
        m_index = 0;

        show();
        raise();
        buildEntryRow();
        buildHeading();
        buildArrowAndSubtitle();

        auto* eff = new QGraphicsOpacityEffect(this);
        setGraphicsEffect(eff);
        auto* fadeIn = new QPropertyAnimation(eff, "opacity", this);
        fadeIn->setStartValue(0.0);
        fadeIn->setEndValue(1.0);
        fadeIn->setDuration(200);
        connect(fadeIn, &QPropertyAnimation::finished, this, [this]() {
            setGraphicsEffect(nullptr);
            flyEntriesToTop();
            });
        fadeIn->start(QAbstractAnimation::DeleteWhenStopped);
    }

private:
    // Small label per pending change, starting stacked near the bottom
    // (roughly where the footer used to sit) — these fly up in step 1.
    void buildEntryRow() {
        const int startY = height() - 80;
        int i = 0;
        for (const PendingChange& c : m_changes) {
            auto* lbl = new QLabel(this);
            lbl->setFont(QFont(Theme::fontFamily(), 8));
            lbl->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::MUTED));
            lbl->setText(c.label + ": " + c.oldValue.toString() + " \u2192 " + c.newValue.toString());
            lbl->adjustSize();
            lbl->move((width() - lbl->width()) / 2, startY - i * 4);
            lbl->show();
            m_entryLabels.append(lbl);
            ++i;
        }
    }

    // Small counter label — this is what visually "becomes" the heading.
    void buildHeading() {
        m_heading = new QLabel(this);
        m_heading->setAlignment(Qt::AlignCenter);
        const QString countText = QString::number(m_changes.size()) +
            (m_changes.size() == 1 ? " pending change" : " pending changes");
        m_heading->setText(countText);
        m_heading->setFont(QFont(Theme::fontFamily(), 9));
        m_heading->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::ACCENT()));
        m_heading->adjustSize();
        m_heading->move((width() - m_heading->width()) / 2, height() - 40);
        m_heading->show();
    }

    void buildArrowAndSubtitle() {
        m_arrow = new QLabel("\u2193", this);
        m_arrow->setFont(QFont(Theme::fontFamily(), 18, QFont::Bold));
        m_arrow->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::ACCENT()));
        m_arrow->adjustSize();
        m_arrow->hide();

        m_subtitle = new QLabel(this);
        m_subtitle->setAlignment(Qt::AlignCenter);
        m_subtitle->setFont(QFont(Theme::fontFamily(), 9));
        m_subtitle->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::MUTED));
        m_subtitle->hide();
    }

    // Step 1 — entries fly from bottom to a centered horizontal row near the top.
    void flyEntriesToTop() {
        const int topY = 60, spacing = 24;
        int totalWidth = 0;
        for (QLabel* l : m_entryLabels) totalWidth += l->width() + spacing;
        if (totalWidth > 0) totalWidth -= spacing;
        int x = (width() - totalWidth) / 2;

        auto* group = new QParallelAnimationGroup(this);
        for (QLabel* l : m_entryLabels) {
            auto* anim = new QPropertyAnimation(l, "pos", this);
            anim->setDuration(420);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            anim->setStartValue(l->pos());
            anim->setEndValue(QPoint(x, topY));
            group->addAnimation(anim);
            x += l->width() + spacing;
        }
        connect(group, &QParallelAnimationGroup::finished, this, [this]() { expandHeading(); });
        group->start(QAbstractAnimation::DeleteWhenStopped);
    }

    // Step 2 — the small counter label grows into the centered page heading.
    void expandHeading() {
        auto* posAnim = new QPropertyAnimation(m_heading, "pos", this);
        posAnim->setDuration(380);
        posAnim->setEasingCurve(QEasingCurve::OutCubic);
        posAnim->setStartValue(m_heading->pos());
        posAnim->setEndValue(QPoint((width() - m_heading->width()) / 2, 140));

        connect(posAnim, &QPropertyAnimation::finished, this, [this]() {
            // Font size isn't animatable — grow it in one step right as the
            // move lands, then re-center for the new (larger) width.
            m_heading->setFont(QFont(Theme::fontFamily(), 20, QFont::Bold));
            m_heading->setText("APPLYING CHANGES");
            m_heading->adjustSize();
            m_heading->move((width() - m_heading->width()) / 2, 120);

            auto* glow = new QGraphicsDropShadowEffect(m_heading);
            glow->setBlurRadius(20);
            glow->setOffset(0, 0);
            glow->setColor(QColor(Theme::ACCENT()));
            m_heading->setGraphicsEffect(glow);

            m_arrow->show();
            m_subtitle->show();
            processNext();
            });
        posAnim->start(QAbstractAnimation::DeleteWhenStopped);
    }

    // Step 3 — walk through each change: point the arrow at its entry label,
    // show applyingLabel, then appliedLabel, then advance.
    void processNext() {
        if (m_index >= m_changes.size()) { finish(); return; }

        const PendingChange change = m_changes[m_index];
        QLabel* target = m_entryLabels[m_index];
        const QPoint arrowPos(target->x() + target->width() / 2 - m_arrow->width() / 2,
            target->y() + target->height() + 6);

        auto* move = new QPropertyAnimation(m_arrow, "pos", this);
        move->setDuration(260);
        move->setEasingCurve(QEasingCurve::OutCubic);
        move->setStartValue(m_arrow->isVisible() ? m_arrow->pos() : arrowPos);
        move->setEndValue(arrowPos);

        const QString applyingText = change.applyingLabel.isEmpty()
            ? QString("Applying %1\u2026").arg(change.label)
            : change.applyingLabel;

        m_subtitle->setText(applyingText);
        m_subtitle->adjustSize();
        m_subtitle->move((width() - m_subtitle->width()) / 2, 175);
        target->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::TEXT));

        connect(move, &QPropertyAnimation::finished, this, [this, change, target]() {
            QTimer::singleShot(260, this, [this, change, target]() {
                const QString appliedText = change.appliedLabel.isEmpty()
                    ? QString("%1 updated").arg(change.label)
                    : change.appliedLabel;

                m_subtitle->setText(appliedText);
                m_subtitle->adjustSize();
                m_subtitle->move((width() - m_subtitle->width()) / 2, 175);
                target->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::ACCENT()));
                QTimer::singleShot(260, this, [this]() { ++m_index; processNext(); });
                });
            });
        move->start(QAbstractAnimation::DeleteWhenStopped);
    }

    // Step 4 — done. First time only: show the disable-hint + Continue button.
    void finish() {
        m_arrow->hide();
        m_subtitle->setText("All changes applied");
        m_subtitle->adjustSize();
        m_subtitle->move((width() - m_subtitle->width()) / 2, 175);

        auto proceed = [this]() { if (m_onComplete) m_onComplete(); deleteLater(); };

        if (!m_showDisableHint) { QTimer::singleShot(700, this, proceed); return; }

        auto* hint = new QLabel(this);
        hint->setWordWrap(true);
        hint->setAlignment(Qt::AlignCenter);
        hint->setText(
            "This animation will be disabled by default. To enable it, go to "
            "Settings \u2192 Appearance \u2192 Animations and set \u201cWhen to show "
            "setting apply animation\u201d to your liking.");
        hint->setFont(QFont(Theme::fontFamily(), 9));
        hint->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::MUTED));
        hint->setFixedWidth(420);
        hint->move((width() - hint->width()) / 2, 220);
        hint->show();

        auto* continueBtn = new QPushButton("Continue", this);
        continueBtn->setFont(QFont(Theme::fontFamily(), 9, QFont::Bold));
        continueBtn->setCursor(Qt::PointingHandCursor);
        continueBtn->setFixedSize(120, 32);
        continueBtn->setStyleSheet(QString(
            "QPushButton{background:%1;border:none;color:#000;border-radius:6px;}"
            "QPushButton:hover{background:%2;}"
        ).arg(Theme::ACCENT(), Theme::ACCENT_DIM));
        continueBtn->move((width() - continueBtn->width()) / 2, 300);
        connect(continueBtn, &QPushButton::clicked, this, proceed);
        continueBtn->show();
    }

    QList<PendingChange>  m_changes;
    int                   m_index = 0;
    bool                  m_showDisableHint = false;
    std::function<void()> m_onComplete;
    QLabel* m_heading = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_arrow = nullptr;
    QList<QLabel*> m_entryLabels;
};


// -- SettingsPage --------------------------------------------------------------

SettingsPage::SettingsPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;").arg(Theme::BG));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    buildChrome();
    root->addWidget(m_chrome);

    // Content area — scrollable
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent; border:none;");

    m_contentStack = new QStackedWidget;
    m_contentStack->setStyleSheet("background:transparent;");
    scroll->setWidget(m_contentStack);
    root->addWidget(scroll, 1);

    // One-shot hint — "Changes apply automatically when you leave this page."
    // Shown the very first time the pending queue goes from empty → non-empty.
    m_pendingHintBar = new QLabel(
        "Changes will automatically be applied once you leave the page.");
    m_pendingHintBar->setFont(MF(8));
    m_pendingHintBar->setStyleSheet(QString(
        "color:%1; background:%2; padding:6px 16px; border-top:1px solid %3;"
    ).arg(Theme::MUTED, Theme::SURFACE, Theme::BORDER));
    m_pendingHintBar->setAlignment(Qt::AlignCenter);
    m_pendingHintBar->hide();
    root->addWidget(m_pendingHintBar);

    connect(&Settings::instance(), &Settings::pendingHintNeeded, this, [this]() {
        fadeWidget(m_pendingHintBar, true, 200);
        Settings::instance().markPendingHintSeen();
        });

    // Footer — bottom right, outside the scroll area
    m_footer = new PendingQueueFooter(this);
    m_footer->setStyleSheet(QString(
        "background:%1; border-top:1px solid %2;"
    ).arg(Theme::SURFACE, Theme::BORDER));
    root->addWidget(m_footer);

    connect(m_footer, &PendingQueueFooter::applySequenceFinishedFirstTime, this, [this]() {
        // Reuse the pending hint bar to deliver the one-shot redirect message.
        m_pendingHintBar->setText(
            "This animation will be disabled by default. To enable it, go to "
            "Behavior → Animations → Show apply animation.");
        fadeWidget(m_pendingHintBar, true, 200);
        });

    // Build initial category view
    buildCategoryView();
    m_contentStack->setCurrentWidget(m_categoryView);
}

void SettingsPage::buildChrome() {
    m_chrome = new QWidget;
    m_chrome->setStyleSheet("background:transparent; border:none;");
    m_chrome->setMinimumHeight(110);

    auto* layout = new QVBoxLayout(m_chrome);
    layout->setContentsMargins(24, 8, 24, 8);
    layout->setSpacing(6);

    

    // Search bar — top of the page, centered, no grey backdrop
    m_searchBar = new QLineEdit;
    m_searchBar->setPlaceholderText("Search settings...");
    m_searchBar->setFont(MF(9));
    m_searchBar->setFixedHeight(30);
    m_searchBar->setFixedWidth(300);
    m_searchBar->setStyleSheet(QString(
        "QLineEdit { background:transparent; border:1px solid %1; border-radius:6px;"
        "color:%2; padding:0 10px; }"
        "QLineEdit:focus { border-color:%3; }"
    ).arg(Theme::BORDER, Theme::TEXT, Theme::ACCENT()));
    connect(m_searchBar, &QLineEdit::textChanged, this, &SettingsPage::onSearchChanged);

    auto* searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->addStretch(1);
    searchRow->addWidget(m_searchBar);
    searchRow->addStretch(1);
    layout->addLayout(searchRow);

    // Visibility control — directly below the search bar, also centered
    m_visControl = new VisibilityControl;
    connect(m_visControl, &VisibilityControl::levelChanged,
        this, &SettingsPage::onVisibilityChanged);

    auto* visRow = new QHBoxLayout;
    visRow->setContentsMargins(0, 0, 0, 0);
    visRow->addStretch(1);
    visRow->addWidget(m_visControl, 0, Qt::AlignCenter);
    visRow->addStretch(1);
    layout->addLayout(visRow);

    // Address bar — below search/visibility, left aligned
    m_addressBar = new QLabel("Settings");
    m_addressBar->setFont(MF(9));
    m_addressBar->setStyleSheet(
        QString("color:%1; background:transparent;").arg(Theme::MUTED));
    layout->addWidget(m_addressBar, 0, Qt::AlignLeft);
}

void SettingsPage::buildCategoryView() {
    m_categoryView = new QWidget;
    m_categoryView->setStyleSheet("background:transparent;");

    auto* layout = new QVBoxLayout(m_categoryView);
    layout->setContentsMargins(24, 32, 24, 32);
    layout->setSpacing(16);

    // Centered heading — all caps, DejaVu Sans, accent green with glow
    auto* heading = new QLabel("SETTINGS");
    QFont headingFont("DejaVu Sans", 22, QFont::Bold);
    heading->setFont(headingFont);
    heading->setStyleSheet(
        QString("color:%1; background:transparent; letter-spacing:4px;").arg(Theme::ACCENT()));
    heading->setAlignment(Qt::AlignCenter);

    auto* headingGlow = new QGraphicsDropShadowEffect(heading);
    headingGlow->setBlurRadius(24);
    headingGlow->setOffset(0, 0);
    headingGlow->setColor(QColor(Theme::ACCENT()));
    heading->setGraphicsEffect(headingGlow);

    layout->addWidget(heading);
    layout->addSpacing(16);

    // Four category cards side by side
    auto* cardRow = new QHBoxLayout;
    cardRow->setSpacing(16);

    for (const CategoryDef& cat : allCategories()) {
        auto* card = new CategoryCard(cat);
        connect(card, &CategoryCard::clicked, this, &SettingsPage::onCategoryClicked);
        cardRow->addWidget(card, 0, Qt::AlignTop);
    }

    layout->addLayout(cardRow, 1);

    m_contentStack->addWidget(m_categoryView);
}

void SettingsPage::buildSubcategoryView(CategoryId cat) {
    // Remove and delete old subcategory view if it exists
    if (m_subcategoryView) {
        m_contentStack->removeWidget(m_subcategoryView);
        delete m_subcategoryView;
    }

    m_subcategoryView = new QWidget;
    m_subcategoryView->setStyleSheet("background:transparent;");

    auto* layout = new QVBoxLayout(m_subcategoryView);
    layout->setContentsMargins(24, 32, 24, 32);
    layout->setSpacing(16);

    // Find the category definition for the accent color
    const CategoryDef* catDef = nullptr;
    for (const CategoryDef& c : allCategories()) {
        if (c.id == cat) { catDef = &c; break; }
    }

    // Back button — top left, returns to category overview (level 0)
    auto* backBtn = new QPushButton("← Settings");
    backBtn->setFont(MF(9));
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFixedHeight(28);
    backBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%2;"
        "border-radius:6px; padding:2px 12px; }"
        "QPushButton:hover { border-color:%3; color:%3; }"
    ).arg(Theme::BORDER, Theme::MUTED, Theme::ACCENT()));
    connect(backBtn, &QPushButton::clicked, this, &SettingsPage::onBackClicked);

    auto* backRow = new QHBoxLayout;
    backRow->addWidget(backBtn, 0, Qt::AlignLeft);
    backRow->addStretch(1);
    layout->addLayout(backRow);

    // Heading in category accent color
    auto* heading = new QLabel(catDef ? catDef->label : "");
    heading->setFont(MF(16, QFont::Bold));
    heading->setStyleSheet(QString("color:%1; background:transparent;")
        .arg(catDef ? catDef->accentColor : Theme::TEXT));
    heading->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);
    layout->addSpacing(8);

    // Subcategory cards — capped at 3 per category for layout consistency
    auto* cardRow = new QHBoxLayout;
    cardRow->setSpacing(16);

    const QList<SubcategoryDef> subs = subcategoriesFor(cat);
    const int maxCards = qMin(subs.size(), 3);

    for (int i = 0; i < maxCards; ++i) {
        auto* card = new SubcategoryCard(subs[i], catDef ? catDef->accentColor : Theme::ACCENT());
        connect(card, &SubcategoryCard::clicked, this, &SettingsPage::onSubcategoryClicked);
        cardRow->addWidget(card);
    }
    // Fill remaining space if fewer than 3 subcategories
    for (int i = maxCards; i < 3; ++i) cardRow->addStretch(1);

    layout->addLayout(cardRow);
    layout->addStretch(1);

    m_contentStack->addWidget(m_subcategoryView);
}

void SettingsPage::buildSettingView(SubcategoryId sub) {
    if (m_settingView) {
        m_contentStack->removeWidget(m_settingView);
        delete m_settingView;
    }

    m_settingView = new QWidget;
    m_settingView->setStyleSheet("background:transparent;");

    auto* layout = new QVBoxLayout(m_settingView);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(0);

    // Find subcategory label for the heading
    const SubcategoryDef* subDef = nullptr;
    for (const SubcategoryDef& s : allSubcategories()) {
        if (s.id == sub) { subDef = &s; break; }
    }

    // Find parent category's accent color for the back button
    QString backAccent = Theme::ACCENT();
    for (const CategoryDef& c : allCategories()) {
        if (subDef && c.id == subDef->parentCategory) { backAccent = c.accentColor; break; }
    }

    // Back button — top left, returns to subcategory overview (level 1)
    auto* backBtn = new QPushButton(subDef ? "← " + categoryLabel(subDef->parentCategory) : "← Back");
    backBtn->setFont(MF(9));
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFixedHeight(28);
    backBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%2;"
        "border-radius:6px; padding:2px 12px; }"
        "QPushButton:hover { border-color:%3; color:%3; }"
    ).arg(Theme::BORDER, Theme::MUTED, backAccent));
    connect(backBtn, &QPushButton::clicked, this, &SettingsPage::onBackClicked);

    auto* backRow = new QHBoxLayout;
    backRow->addWidget(backBtn, 0, Qt::AlignLeft);
    backRow->addStretch(1);
    layout->addLayout(backRow);
    layout->addSpacing(8);

    auto* heading = new QLabel(subDef ? subDef->label : "");
    heading->setFont(MF(14, QFont::Bold));
    heading->setStyleSheet(
        QString("color:%1; background:transparent;").arg(Theme::TEXT));
    heading->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);
    layout->addSpacing(16);

    // Left accent border container
    auto* listContainer = new QWidget;
    listContainer->setStyleSheet(QString(
        "background:%1; border-radius:10px; border:1px solid %2;"
    ).arg(Theme::SURFACE, Theme::BORDER));

    auto* listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    // Setting rows
    const QList<SettingDef> settings = settingsFor(sub,
        Settings::instance().visibilityLevel());

    for (const SettingDef& def : settings) {
        auto* row = new SettingRow(def);
        connect(row, &SettingRow::actionTriggered, this, &SettingsPage::onActionTriggered);
        listLayout->addWidget(row);
    }
    listLayout->addStretch(1);

    layout->addWidget(listContainer, 1);

    m_contentStack->addWidget(m_settingView);
}

void SettingsPage::buildSearchView(const QString& query) {
    if (m_searchView) {
        m_contentStack->removeWidget(m_searchView);
        delete m_searchView;
        m_searchView = nullptr;
    }

    m_searchView = new QWidget;
    m_searchView->setStyleSheet("background:transparent;");

    auto* layout = new QVBoxLayout(m_searchView);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(0);

    const QList<SettingDef> results = searchSettings(query,
        Settings::instance().visibilityLevel());

    if (results.isEmpty()) {
        auto* empty = new QLabel("No settings found");
        empty->setFont(MF(10));
        empty->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::MUTED));
        empty->setAlignment(Qt::AlignCenter);
        layout->addStretch(1);
        layout->addWidget(empty);
        layout->addStretch(1);
    }
    else {
        auto* listContainer = new QWidget;
        listContainer->setStyleSheet(QString(
            "background:%1; border-radius:10px; border:1px solid %2;"
        ).arg(Theme::SURFACE, Theme::BORDER));

        auto* listLayout = new QVBoxLayout(listContainer);
        listLayout->setContentsMargins(0, 0, 0, 0);
        listLayout->setSpacing(0);

        for (const SettingDef& def : results) {
            auto* row = new SearchResultRow(def);
            connect(row, &SearchResultRow::clicked, this, [this](SettingDef def) {
                // Clear the search so leaving search mode doesn't re-trigger it
                m_searchBar->blockSignals(true);
                m_searchBar->clear();
                m_searchBar->blockSignals(false);

                m_activeCategory = def.category;
                m_activeSub = def.subcategory;
                m_currentLevel = 2;
                buildSettingView(def.subcategory);
                navigateTo(2);
                updateAddressBar();
                });
            listLayout->addWidget(row);
        }
        listLayout->addStretch(1);

        layout->addWidget(listContainer, 1);
    }

    m_contentStack->addWidget(m_searchView);
}

// -- Navigation ----------------------------------------------------------------

void SettingsPage::onCategoryClicked(CategoryId cat) {
    // Apply any pending changes before leaving the current subcategory view
    if (m_currentLevel == 2)
        Settings::instance().applyPending(false);

    m_activeCategory = cat;
    m_currentLevel = 1;
    buildSubcategoryView(cat);
    navigateTo(1);
    updateAddressBar();

    if (Settings::instance().hintState() == HintState::Active)
        Settings::instance().advanceHintState(HintState::Passive);
}

void SettingsPage::onSubcategoryClicked(SubcategoryId sub) {
    // Apply pending changes when switching between subcategories at level 2
    if (m_currentLevel == 2)
        Settings::instance().applyPending(false);

    m_activeSub = sub;
    m_currentLevel = 2;
    buildSettingView(sub);
    navigateTo(2);
    updateAddressBar();
}
void SettingsPage::onBackClicked() {
    if (m_currentLevel == 0) return;
    m_currentLevel--;
    navigateTo(m_currentLevel);
    updateAddressBar();
}

void SettingsPage::navigateTo(int level) {
    QWidget* target = nullptr;
    switch (level) {
    case 0: target = m_categoryView;    break;
    case 1: target = m_subcategoryView; break;
    case 2: target = m_settingView;     break;
    default: return;
    }
    if (!target) return;

    QWidget* current = m_contentStack->currentWidget();
    if (current == target) return;

    fadeWidget(current, false, 150);
    m_contentStack->setCurrentWidget(target);
    fadeWidget(target, true, 180);
}


void SettingsPage::updateAddressBar() {
    QString path = "<span style='color:" + Theme::TEXT + ";'>Settings</span>";

    if (m_currentLevel >= 1) {
        for (const CategoryDef& c : allCategories()) {
            if (c.id == m_activeCategory) {
                path += "<span style='color:" + Theme::MUTED + ";'> → </span>"
                    + "<span style='color:" + c.accentColor + ";'>" + c.label + "</span>";
                break;
            }
        }
    }
    if (m_currentLevel >= 2) {
        for (const SubcategoryDef& s : allSubcategories()) {
            if (s.id == m_activeSub) {
                path += "<span style='color:" + Theme::MUTED + ";'> → </span>"
                    + "<span style='color:" + Theme::TEXT + ";'>" + s.label + "</span>";
                break;
            }
        }
    }

    m_addressBar->setText(path);
}

void SettingsPage::onSearchChanged(const QString& query) {
    
        if (query.trimmed().isEmpty()) {
            if (m_searchView) {
                m_contentStack->removeWidget(m_searchView);
                delete m_searchView;
                m_searchView = nullptr;
            }
            // Rebuild the target view in case navigation happened during search
            switch (m_currentLevel) {
            case 1: buildSubcategoryView(m_activeCategory); break;
            case 2: buildSettingView(m_activeSub);          break;
            default: break; // level 0 (category view) is never rebuilt, always valid
            }
            updateAddressBar();
            navigateTo(m_currentLevel);
            return;
        }

    buildSearchView(query);

    const QList<SettingDef> results = searchSettings(query,
        Settings::instance().visibilityLevel());

    m_addressBar->setText(
        QString("<span style='color:%1;'>Search: </span>"
            "<span style='color:%2;'>\"%3\" — %4 result%5</span>")
        .arg(Theme::MUTED, Theme::TEXT)
        .arg(query.toHtmlEscaped())
        .arg(results.size())
        .arg(results.size() == 1 ? "" : "s"));

    // Fade directly to the search view regardless of current level
    QWidget* current = m_contentStack->currentWidget();
    if (current != m_searchView) {
        fadeWidget(current, false, 120);
        m_contentStack->setCurrentWidget(m_searchView);
        fadeWidget(m_searchView, true, 150);
    }
    else {
        // Already showing a (now-replaced) search view — just fade in the new one
        fadeWidget(m_searchView, true, 150);
    }
}

void SettingsPage::onVisibilityChanged(VisibilityLevel level) {
    Q_UNUSED(level)
        // Rebuild current view to reflect new visibility level
        if (m_currentLevel == 2)
            buildSettingView(m_activeSub);
}

void SettingsPage::onPendingChanged() {
    // Footer handles its own rebuild via its own connection
}

void SettingsPage::onActionTriggered(QString key) {
    if (key == "behavior/memory/copyHistory") {
        // SettingsPage has no access to MainWindow::m_history — bubble the
        // request up so MainWindow can do the actual clipboard write.
        emit copyHistoryRequested();
        return;
    }
    if (key == "system/data/exportSettings") {
        const QString path = QFileDialog::getSaveFileName(
            this, "Export Settings", "mathx-settings.json", "JSON Files (*.json)");
        if (path.isEmpty()) return;

        QJsonObject obj;
        for (const SettingDef& def : allSettings())
            obj[def.key] = QJsonValue::fromVariant(Settings::instance().get(def.key));

        QFile f(path);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
            f.close();
        }
    }
    else if (key == "system/data/importSettings") {
        const QString path = QFileDialog::getOpenFileName(
            this, "Import Settings", QString(), "JSON Files (*.json)");
        if (path.isEmpty()) return;

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) return;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isObject()) return;

        const QJsonObject obj = doc.object();
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            // Only import keys that are actually registered settings —
            // ignores stale/unknown keys from older or hand-edited files.
            if (findSetting(it.key()))
                Settings::instance().set(it.key(), it.value().toVariant());
        }
        Settings::instance().applyPending(true);

        // Refresh whatever's currently on screen so values update visibly
        if (m_currentLevel == 2) buildSettingView(m_activeSub);
    }
    else if (key == "system/data/resetAll") {
        auto reply = QMessageBox::question(
            this, "Reset all settings",
            "This will reset every setting back to its default value. This cannot be undone.\n\nContinue?",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (reply != QMessageBox::Yes) return;

        Settings::instance().resetAll();
        if (m_currentLevel == 2) buildSettingView(m_activeSub);
    }
}
void SettingsPage::prepareToLeave(std::function<void()> onComplete) {
    if (!Settings::instance().hasPendingChanges()) {
        if (onComplete) onComplete();
        return;
    }

    // Snapshot before applying — applyPending() clears the queue, so grab
    // what we need for the animation first.
    const QList<PendingChange> snapshot = Settings::instance().pendingChanges();
    const bool wasOnce = (Settings::instance().animationMode() == AnimationMode::Once);
    const bool playAnim = Settings::instance().shouldPlayApplyAnimation();

    Settings::instance().applyPending(true);

    const bool showDisableHint = wasOnce && !Settings::instance().hasSeenPostAnimationHint();
    if (showDisableHint) Settings::instance().markPostAnimationHintSeen();

    if (!playAnim) {
        if (onComplete) onComplete();
        return;
    }

    fadeWidget(m_chrome, false, 150);
    fadeWidget(m_contentStack, false, 150);
    fadeWidget(m_footer, false, 150);

    auto* overlay = new ApplyAnimationOverlay(this);
    overlay->setGeometry(rect());

    overlay->play(snapshot, showDisableHint, [this, onComplete]() {
        m_chrome->show();
        m_contentStack->show();
        m_footer->show();
        fadeWidget(m_chrome, true, 1);
        fadeWidget(m_contentStack, true, 1);
        fadeWidget(m_footer, true, 1);
        if (onComplete) onComplete();
        });
}

// Required because Q_OBJECT classes (CategoryCard, SubcategoryCard, SettingRow,
// VisibilityControl, PendingQueueFooter) are defined inline in this .cpp file
// rather than in their own headers. MOC needs this to find them.
#include "SettingsPage.moc"