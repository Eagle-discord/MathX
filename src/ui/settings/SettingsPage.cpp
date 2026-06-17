#include "SettingsPage.h"
#include "../Animations.h"
#include "../../constants/Theme.h"
#include "../../settings/SettingsDef.h"
#include <QScrollArea>
#include <QGraphicsOpacityEffect>
#include <QGraphicsDropShadowEffect>
#include <QPropertyAnimation>
#include <QSizePolicy>
#include <QPainter>
#include <QMouseEvent>
#include <QResizeEvent>

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

        // Control placeholder — will be replaced by actual control widgets
        m_controlPlaceholder = new QLabel("—");
        m_controlPlaceholder->setFont(MF(9));
        m_controlPlaceholder->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::MUTED));
        m_controlPlaceholder->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        layout->addWidget(m_controlPlaceholder);

        refreshForLevel(Settings::instance().visibilityLevel());

        connect(&Settings::instance(), &Settings::visibilityLevelChanged,
            this, &SettingRow::refreshForLevel);

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
    SettingDef  m_def;
    QLabel* m_label = nullptr;
    QLabel* m_desc = nullptr;
    QLabel* m_controlPlaceholder = nullptr;
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
        QString accent = Theme::ACCENT;
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
    ).arg(Theme::ACCENT, Theme::ACCENT_DIM));
    m_helpBtn->hide();
    showHintText();
}

void VisibilityControl::enterPassiveState() {
    m_levelBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%2;"
        "border-radius:6px; padding:2px 10px; }"
        "QPushButton:hover { border-color:%3; color:%3; }"
    ).arg(Theme::BORDER, Theme::TEXT, Theme::ACCENT));
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

        auto* oldLbl = new QLabel(change.oldValue.toString());
        oldLbl->setFont(MF(8));
        oldLbl->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::TEXT));

        auto* newLbl = new QLabel(change.newValue.toString());
        newLbl->setFont(MF(8, QFont::Bold));
        newLbl->setStyleSheet(
            QString("color:%1; background:transparent;").arg(Theme::ACCENT));

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
        count > 0 ? Theme::ACCENT : Theme::MUTED));

    m_queueList->setVisible(count > 0);
}

void PendingQueueFooter::playApplySequence(std::function<void()> onComplete) {
    if (!Settings::instance().hasPendingChanges()) {
        if (onComplete) onComplete();
        return;
    }
    // Apply animation handled by SettingsAnimations (future module).
    // For now, apply directly and call onComplete.
    Settings::instance().applyPending(true);
    if (onComplete) onComplete();
}

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

    // Footer — bottom right, outside the scroll area
    m_footer = new PendingQueueFooter(this);
    m_footer->setStyleSheet(QString(
        "background:%1; border-top:1px solid %2;"
    ).arg(Theme::SURFACE, Theme::BORDER));
    root->addWidget(m_footer);

    // Build initial category view
    buildCategoryView();
    m_contentStack->setCurrentWidget(m_categoryView);
}

void SettingsPage::buildChrome() {
    m_chrome = new QWidget;
    m_chrome->setStyleSheet("background:transparent; border:none;");
    m_chrome->setFixedHeight(110);

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
    ).arg(Theme::BORDER, Theme::TEXT, Theme::ACCENT));
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
        QString("color:%1; background:transparent; letter-spacing:4px;").arg(Theme::ACCENT));
    heading->setAlignment(Qt::AlignCenter);

    auto* headingGlow = new QGraphicsDropShadowEffect(heading);
    headingGlow->setBlurRadius(24);
    headingGlow->setOffset(0, 0);
    headingGlow->setColor(QColor(Theme::ACCENT));
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
    ).arg(Theme::BORDER, Theme::MUTED, Theme::ACCENT));
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
        auto* card = new SubcategoryCard(subs[i], catDef ? catDef->accentColor : Theme::ACCENT);
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
    QString backAccent = Theme::ACCENT;
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
            "background:%1; border-radius:10px;"
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
    m_activeCategory = cat;
    m_currentLevel = 1;
    buildSubcategoryView(cat);
    navigateTo(1);
    updateAddressBar();

    // Advance hint from Active to Passive on first interaction
    if (Settings::instance().hintState() == HintState::Active)
        Settings::instance().advanceHintState(HintState::Passive);
}

void SettingsPage::onSubcategoryClicked(SubcategoryId sub) {
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

    // Apply any pending changes before changing category (level 1 → 1 transition)
    if (m_currentLevel == 1 && level == 1) {
        Settings::instance().applyPending(false);
    }

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
        // Clean up search view and return to current level
        if (m_searchView) {
            m_contentStack->removeWidget(m_searchView);
            delete m_searchView;
            m_searchView = nullptr;
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

void SettingsPage::prepareToLeave(std::function<void()> onComplete) {

    m_footer->playApplySequence(onComplete);
}

// Required because Q_OBJECT classes (CategoryCard, SubcategoryCard, SettingRow,
// VisibilityControl, PendingQueueFooter) are defined inline in this .cpp file
// rather than in their own headers. MOC needs this to find them.
#include "SettingsPage.moc"