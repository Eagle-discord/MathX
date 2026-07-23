#pragma once
#include <QObject>
#include <QWidget>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QFont>
#include <optional>
#include "../settings/Settings.h"   // for VisibilityLevel - adjust path as needed

// -- WidgetStyle ---------------------------------------------------------------
// Per-widget override record. Every field is optional:
//   nullopt  → inherit the current global value
//   has_value → this widget ignores the global value for that property
struct WidgetStyle {
    std::optional<QString> accentColor;
    std::optional<QString> stylesheet;   // full QSS - replaces role-derived sheet
    std::optional<int>     fontSize;
    std::optional<QString> fontFamily;
    std::optional<bool>    visible;
};

// -- WidgetRole ----------------------------------------------------------------
// Semantic role flags - assigned at registration time, drive what the global
// refresh does to this widget. Roles are combined with operator|.
// WidgetRegistry.h
enum class WidgetRole : uint32_t {
    None = 0,
    Text = 1 << 0,
    AccentFill = 1 << 1,
    AccentBorder = 1 << 2,
    AccentText = 1 << 3,
    Muted = 1 << 4,
    Surface = 1 << 5,
    Card = 1 << 6,
    FontUI = 1 << 7,
    FontResults = 1 << 11,
    FontSeparators = 1 << 12,
    FontInput = 1 << 13,
    FontSidebar = 1 << 14,
    FontSplash = 1 << 15,  // terminal splash screen text (title/instructions/examples)
    BasicOnly = 1 << 8,
    AdvancedPlus = 1 << 9,
    DeveloperOnly = 1 << 10,
};
// Back-compat alias - every existing WR_ADD(..., WidgetRole::FontUI) call
// site still compiles and now maps to the general "Buttons & labels" bucket.
// New call sites should prefer a specific Font* role where one applies.
inline constexpr WidgetRole MonoFontCompat = WidgetRole::FontUI;
#define MonoFont FontUI
inline constexpr WidgetRole operator|(WidgetRole a, WidgetRole b) {
    return static_cast<WidgetRole>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasRole(WidgetRole set, WidgetRole flag) {
    return (static_cast<uint32_t>(set) & static_cast<uint32_t>(flag)) != 0;
}

// -- WidgetRegistry ------------------------------------------------------------
// Singleton. Every widget registered here receives live style updates when
// Settings signals fire. Widgets auto-deregister when destroyed.
//
// Quick usage:
//   int id = WidgetRegistry::instance().add(btn, WidgetRole::AccentFill | WidgetRole::FontUI);
//   WidgetRegistry::instance().setAccentColor(id, "#ff0000"); // per-widget override
//   WidgetRegistry::instance().clearStyle(id);                // remove override
//   WidgetRegistry::instance().refreshAll();                  // force full repaint

class WidgetRegistry : public QObject {
    Q_OBJECT
public:

    static WidgetRegistry& instance();

    // -- Registration ----------------------------------------------------------
    // Returns a stable int ID. Widget auto-deregisters on destroy.
    // applyNow=true immediately paints the widget with the current global style.
    int  add(QWidget* widget, WidgetRole roles = WidgetRole::None, bool applyNow = true);
    void remove(int id);

    // -- Per-widget overrides (individual always wins over global) -------------
    void setStyle(int id, const WidgetStyle& style);
    void setAccentColor(int id, const QString& color);
    void setStylesheet(int id, const QString& qss);
    void setFontSize(int id, int pt);
    void setFontFamily(int id, const QString& family);
    void setVisible(int id, bool visible);
    void clearStyle(int id);   // remove all overrides, fall back to global

    // -- Global refresh --------------------------------------------------------
    void refreshAll();         // re-apply current global style to every widget
    void refresh(int id);      // re-apply to one widget
    void applyVisibilityLevel(VisibilityLevel level);

private:
    explicit WidgetRegistry(QObject* parent = nullptr);
    void refreshFontRoles();

    struct Entry {
        int               id = 0;
        QPointer<QWidget> widget;
        WidgetRole        roles = WidgetRole::None;
        WidgetStyle       override;
        bool              hasOverride = false;
    };

    void    applyEntry(Entry& e);
    QString buildStylesheet(const Entry& e) const;
    QFont   buildFont(const Entry& e) const;

    QHash<int, Entry> m_entries;
    int               m_nextId = 1;
    QString           m_globalAccent;
    QString           m_globalFamily;
    QString           m_globalWeight = "Normal";
    int               m_globalSizeUI = 10;
    int               m_globalSizeResults = 9;
    int               m_globalSizeSeparators = 9;
    int               m_globalSizeInput = 10;
    int               m_globalSizeSidebar = 9;
    int               m_globalSizeSplash = 9;
    VisibilityLevel   m_globalLevel = VisibilityLevel::Basic;
};

// -- Convenience macro ---------------------------------------------------------
// WR_ADD(m_runBtnId, m_runBtn, WidgetRole::AccentFill | WidgetRole::FontUI);
#define WR_ADD(idVar, widget, roles) \
    (idVar) = WidgetRegistry::instance().add((widget), (roles))