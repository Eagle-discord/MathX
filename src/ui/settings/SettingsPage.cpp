#include "SettingsPage.h"
#include "../Animations.h"
#include "../../constants/Theme.h"
#include "../../settings/SettingsDef.h"
#include <QScrollArea>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QSizePolicy>
#include <QPainter>
#include <QMouseEvent>

// ── Helpers ───────────────────────────────────────────────────────────────────

static QFont MF(int pt, int w = QFont::Normal) {
    QFont f(Theme::fontFamily());
    f.setPointSize(pt);
    f.setWeight(static_cast<QFont::Weight>(w));
    f.setStyleHint(QFont::Monospace);
    return f;
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

// ── CategoryCard ──────────────────────────────────────────────────────────────
// Inline — small enough to live here rather than its own file

class CategoryCard : public QWidget {
    Q_OBJECT
public:
    explicit CategoryCard(const CategoryDef& def, QWidget* parent = nullptr)
        : QWidget(parent), m_def(def)
    {
        setMinimumHeight(160);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAttribute(Qt::WA_StyledBackground, true);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(6);
        layout->addStretch(1);

        m_label = new QLabel(def.label);
        m_label->setFont(MF(13, QFont::Bold));
        m_label->setStyleSheet(QString("color:%1; background:transparent;").arg(def.accentColor));
        m_label->setAlignment(Qt::AlignCenter);

        m_desc = new QLabel(def.description);
        m_desc->setFont(MF(8));
        m_desc->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::MUTED));
        m_desc->setAlignment(Qt::AlignCenter);
 
        m_desc->setMinimumWidth(160);  // gives wordWrap something to wrap against

        // Single shared backdrop behind both label and description
        auto* textBox = new QWidget;
        textBox->setMinimumWidth(200);
        textBox->setStyleSheet("background:rgba(0,0,0,90); border-radius:8px;");
        auto* textBoxLayout = new QVBoxLayout(textBox);
        textBoxLayout->setContentsMargins(24, 16, 24, 16);
        textBoxLayout->setSpacing(6);
        textBoxLayout->addWidget(m_label);
        textBoxLayout->addWidget(m_desc);

        layout->addWidget(textBox, 0, Qt::AlignCenter);
        layout->addStretch(1);

        updateStyle(false);
    }

signals:
    void clicked(CategoryId id);

protected:
    void mousePressEvent(QMouseEvent*) override { emit clicked(m_def.id); }

    void enterEvent(QEnterEvent*) override { updateStyle(true); }
    void leaveEvent(QEvent*)      override { updateStyle(false); }

    void paintEvent(QPaintEvent* e) override {
        QWidget::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Faded background icon — large unicode symbol centered.
        // setAlphaF on the pen has no effect on color emoji glyphs since
        // they're full-color bitmaps, not outline-rendered text. Use
        // QPainter::setOpacity instead, which applies to the whole draw.
        QFont f = MF(80);
        p.setFont(f);
        p.setOpacity(0.12);
        p.setPen(QColor(m_def.accentColor));
        p.drawText(rect(), Qt::AlignCenter, iconGlyph());

        // Border-only glow — layered translucent rounded outlines, widest
        // and faintest on the outside, narrowing to a crisp 1.5px edge.
        // This keeps the glow confined to the border instead of bleeding
        // across the whole card like a drop shadow would.
        p.setOpacity(1.0);
        p.setBrush(Qt::NoBrush);
        QColor accent(m_def.accentColor);

        const QRectF base = QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75);
        const int glowLayers = m_hovered ? 5 : 3;
        const qreal maxSpread = m_hovered ? 6.0 : 3.0;

        for (int i = glowLayers; i >= 1; --i) {
            const qreal spread = (maxSpread / glowLayers) * i;
            QColor glow = accent;
            glow.setAlphaF((m_hovered ? 0.10 : 0.06) * (1.0 - (qreal)(i - 1) / glowLayers));
            p.setPen(QPen(glow, 1.0 + spread));
            p.drawRoundedRect(base.adjusted(-spread, -spread, spread, spread), 14, 14);
        }

        // Crisp edge on top
        p.setPen(QPen(accent, 1.5));
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

    // Maps iconName to a unicode glyph for the faded background
    QString iconGlyph() const {
        if (m_def.iconName == "brush")   return "🖌";
        if (m_def.iconName == "monitor") return "🖥";
        if (m_def.iconName == "gear")    return "⚙";
        if (m_def.iconName == "server")  return "🖧";
        return "◈";
    }

    CategoryDef  m_def;
    QLabel* m_label = nullptr;
    QLabel* m_desc = nullptr;
    bool         m_hovered = false;
};

// ── SubcategoryCard ───────────────────────────────────────────────────────────

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

// ── SettingRow ────────────────────────────────────────────────────────────────

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
        // in a follow-up when individual control types are implemented
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

// ── VisibilityControl ─────────────────────────────────────────────────────────

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

// ── PendingQueueFooter ────────────────────────────────────────────────────────

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

// ── SettingsPage ──────────────────────────────────────────────────────────────

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
    m_chrome->setStyleSheet(QString(
        "background:%1; border-bottom:1px solid %2;"
    ).arg(Theme::SURFACE, Theme::BORDER));
    m_chrome->setFixedHeight(80);

    auto* layout = new QVBoxLayout(m_chrome);
    layout->setContentsMargins(24, 8, 24, 8);
    layout->setSpacing(6);

    // Row 1 — address bar (left) + spacer (right side reserved for >_ button in MainWindow)
    m_addressBar = new QLabel("Settings");
    m_addressBar->setFont(MF(9));
    m_addressBar->setStyleSheet(
        QString("color:%1; background:transparent;").arg(Theme::MUTED));
    layout->addWidget(m_addressBar, 0, Qt::AlignLeft);

    m_addressBackBtn = new QPushButton("<--");
    m_addressBackBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%2;"
        "border-radius:6px; padding:0px; }"
        "QPushButton:hover { border-color:%2; color:%2; }"
    ).arg(C_BORDER, C_ACCENT));
    connect(m_addressBackBtn, &QPushButton::clicked, this, &SettingsPage::onBackClicked);
    m_addressBackBtn->hide();

    layout->addWidget(m_addressBackBtn, 0, Qt::AlignLeft);
    // Row 2 — search bar (center) + visibility control (right)
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(12);

    row2->addStretch(1);

    m_searchBar = new QLineEdit;
    m_searchBar->setPlaceholderText("Search settings...");
    m_searchBar->setFont(MF(9));
    m_searchBar->setFixedHeight(30);
    m_searchBar->setFixedWidth(300);
    m_searchBar->setStyleSheet(QString(
        "QLineEdit { background:%1; border:1px solid %2; border-radius:6px;"
        "color:%3; padding:0 10px; }"
        "QLineEdit:focus { border-color:%4; }"
    ).arg(Theme::BG, Theme::BORDER, Theme::TEXT, Theme::ACCENT));
    connect(m_searchBar, &QLineEdit::textChanged, this, &SettingsPage::onSearchChanged);
    row2->addWidget(m_searchBar);

    row2->addStretch(1);

    m_visControl = new VisibilityControl;
    connect(m_visControl, &VisibilityControl::levelChanged,
        this, &SettingsPage::onVisibilityChanged);
    row2->addWidget(m_visControl, 0, Qt::AlignRight | Qt::AlignVCenter);

    layout->addLayout(row2);
}

void SettingsPage::buildCategoryView() {
    m_categoryView = new QWidget;
    m_categoryView->setStyleSheet("background:transparent;");

    auto* layout = new QVBoxLayout(m_categoryView);
    layout->setContentsMargins(24, 32, 24, 32);
    layout->setSpacing(16);

    // Centered heading
    auto* heading = new QLabel("Settings");
    heading->setFont(MF(18, QFont::Bold));
    heading->setStyleSheet(
        QString("color:%1; background:transparent;").arg(Theme::TEXT));
    heading->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);
    layout->addSpacing(16);

    // Four category cards side by side
    auto* cardRow = new QHBoxLayout;
    cardRow->setSpacing(16);

    for (const CategoryDef& cat : allCategories()) {
        auto* card = new CategoryCard(cat);
        connect(card, &CategoryCard::clicked, this, &SettingsPage::onCategoryClicked);
        cardRow->addWidget(card);
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

    // Heading in category accent color
    auto* heading = new QLabel(catDef ? catDef->label : "");
    heading->setFont(MF(16, QFont::Bold));
    heading->setStyleSheet(QString("color:%1; background:transparent;")
        .arg(catDef ? catDef->accentColor : Theme::TEXT));
    heading->setAlignment(Qt::AlignCenter);
    layout->addWidget(heading);
    layout->addSpacing(8);

    // Subcategory cards
    auto* cardRow = new QHBoxLayout;
    cardRow->setSpacing(16);

    for (const SubcategoryDef& sub : subcategoriesFor(cat)) {
        auto* card = new SubcategoryCard(sub, catDef ? catDef->accentColor : Theme::ACCENT);
        connect(card, &SubcategoryCard::clicked, this, &SettingsPage::onSubcategoryClicked);
        cardRow->addWidget(card);
    }
    // Fill remaining space if fewer than 4 subcategories
    cardRow->addStretch(1);

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

// ── Navigation ────────────────────────────────────────────────────────────────

void SettingsPage::onCategoryClicked(CategoryId cat) {
    m_activeCategory = cat;
    m_currentLevel = 1;
    m_addressBackBtn->show();
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
    if (m_currentLevel == 0) m_addressBackBtn->hide();
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
        // Return to current level view
        navigateTo(m_currentLevel);
        return;
    }

    // Build a search results view inline
    // Full search results view is a follow-up — for now show result count
    const QList<SettingDef> results = searchSettings(query,
        Settings::instance().visibilityLevel());

    m_addressBar->setText(
        QString("<span style='color:%1;'>Search: </span>"
            "<span style='color:%2;'>%3 result%4</span>")
        .arg(Theme::MUTED, Theme::TEXT)
        .arg(results.size())
        .arg(results.size() == 1 ? "" : "s"));
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