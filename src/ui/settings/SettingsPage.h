#pragma once
#include <QWidget>
#include <functional>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include "../../settings/Settings.h"
#include "../../settings/SettingsDef.h"

// -- SettingsPage --------------------------------------------------------------
// Full-window settings UI. Placed inside MainWindow's page 2.
// Manages its own navigation stack:
//   Level 0 - category overview (four cards)
//   Level 1 - subcategory overview (cards for one category)
//   Level 2 - setting list (all settings for one subcategory)
//
// The address bar, search bar, visibility control, and pending queue footer
// are persistent chrome - always visible regardless of navigation level.
// Only the content area transitions between levels.

class CategoryCard;
class SubcategoryCard;
class SettingRow;
class PendingQueueFooter;
class VisibilityControl;

class SettingsPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingsPage(QWidget* parent = nullptr);

    // Called by MainWindow before navigating away - triggers apply animation
    // then calls onComplete when done. If no pending changes, calls immediately.
    void prepareToLeave(std::function<void()> onComplete);

signals:
    void navigateAway(); // emitted after apply animation completes
    void copyHistoryRequested(); // user triggered "Copy history" - MainWindow has the actual data

private slots:
    void onCategoryClicked(CategoryId cat);
    void onSubcategoryClicked(SubcategoryId sub);
    void onSearchChanged(const QString& query);
    void onVisibilityChanged(VisibilityLevel level);
    void onPendingChanged();
    void onBackClicked();
    void onActionTriggered(QString key); // handles Action-control settings (export/import/reset)

private:
    // -- Layout builders -------------------------------------------------------
    void buildChrome();         // address bar + search + visibility + footer
    void buildCategoryView();   // level 0 - four category cards
    void buildSubcategoryView(CategoryId cat);   // level 1
    void buildSettingView(SubcategoryId sub);     // level 2
    void buildSearchView(const QString& query);  // search results - overlays current level

    // -- Navigation ------------------------------------------------------------
    void navigateTo(int level);  // animates content transition
    void updateAddressBar();

    // -- Address bar helpers ---------------------------------------------------
    void setAddress(const QString& path);  // e.g. "Settings → Appearance → Typography"

    // -- Persistent chrome -----------------------------------------------------
    QWidget* m_chrome = nullptr; // top bar (address + search + visibility)
    QLabel* m_addressBar = nullptr; // "Settings → Appearance → Typography"
    QLineEdit* m_searchBar = nullptr;
    VisibilityControl* m_visControl = nullptr;
    PendingQueueFooter* m_footer = nullptr; // bottom-right pending queue
    QLabel* m_pendingHintBar = nullptr; // one-shot "applies on leave" hint

    // -- Content area ----------------------------------------------------------
    QStackedWidget* m_contentStack = nullptr; // swapped on navigation
    QWidget* m_categoryView = nullptr; // level 0
    QWidget* m_subcategoryView = nullptr; // level 1 (rebuilt per category)
    QWidget* m_settingView = nullptr; // level 2 (rebuilt per subcategory)
    QWidget* m_searchView = nullptr; // search results overlay (rebuilt per query)

    // -- Navigation state ------------------------------------------------------
    int                 m_currentLevel = 0;
    CategoryId          m_activeCategory = CategoryId::Appearance;
    SubcategoryId       m_activeSub = SubcategoryId::Typography;
};


// -- VisibilityControl ---------------------------------------------------------
// The visibility level dropdown + hint system.
// Manages its own Active/Passive/Dormant state driven by Settings::hintState().

class VisibilityControl : public QWidget {
    Q_OBJECT
public:
    explicit VisibilityControl(QWidget* parent = nullptr);

signals:
    void levelChanged(VisibilityLevel level);

private slots:
    void onHintStateChanged(HintState state);

private:
    void enterActiveState();
    void enterPassiveState();
    void enterDormantState();
    void showHintText();
    void hideHintText();

    QPushButton* m_levelBtn = nullptr; // "Basic ▾" dropdown trigger
    QLabel* m_hintLabel = nullptr; // the hint text
    QPushButton* m_helpBtn = nullptr; // ? button in Passive state
    QWidget* m_glow = nullptr; // accent border overlay in Active state
};


// -- PendingQueueFooter --------------------------------------------------------
// Bottom-right footer showing staged/deferred changes.
// Expands to show the full queue. Collapses to a counter when minimal.

class PendingQueueFooter : public QWidget {
    Q_OBJECT
public:
    explicit PendingQueueFooter(QWidget* parent = nullptr);

    // Triggers the apply animation sequence then calls onComplete.
    void playApplySequence(std::function<void()> onComplete);

signals:
    // Emitted once, the first time the Once animation mode auto-downgrades
    // to Never after playing. SettingsPage shows the redirect message.
    void applySequenceFinishedFirstTime();

public slots:
    void onPendingChanged();

private:
    void rebuild();  // rebuilds the queue entry list from Settings::pendingChanges()

    QLabel* m_countLabel = nullptr; // "3 pending changes"
    QWidget* m_queueList = nullptr; // the expandable list of entries
    QVBoxLayout* m_listLayout = nullptr;
};