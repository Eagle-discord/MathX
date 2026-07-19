#include "WidgetRegistry.h"
#include "../settings/Settings.h"
#include <constants/Theme.h>

// -- Singleton -----------------------------------------------------------------

WidgetRegistry& WidgetRegistry::instance() {
    static WidgetRegistry inst;
    return inst;
}

WidgetRegistry::WidgetRegistry(QObject* parent) : QObject(parent) {
    auto& S = Settings::instance();
    m_globalAccent = Theme::accentColor();
    m_globalFamily = Theme::fontFamily();
    m_globalSizeUI = S.fontSizeUI();
    m_globalSizeResults = S.fontSizeResults();
    m_globalSizeSeparators = S.fontSizeSeparators();
    m_globalSizeInput = S.fontSizeInput();
    m_globalSizeSidebar = S.fontSizeSidebar();
    m_globalSizeSplash = S.fontSizeSplash();
    m_globalLevel = S.visibilityLevel();

    connect(&S, &Settings::accentColorChanged, this, [this](const QString& color) {
        m_globalAccent = color;
        refreshAll();
        });

    connect(&S, &Settings::fontFamilyChanged, this, [this](const QString& family) {
        m_globalFamily = family;
        refreshFontRoles();
        });

    /*  connect(&S, &Settings::fontWeightChanged, this, [this](const QString& weight) {
          m_globalWeight = weight;

          refreshFontRoles();
          });
          */
    auto refreshOne = [this](WidgetRole role) {
        for (auto& e : m_entries)
            if (e.widget && hasRole(e.roles, role))
                applyEntry(e);
        };

    connect(&S, &Settings::fontSizeUIChanged, this, [this, refreshOne](int pt) {
        m_globalSizeUI = pt; refreshOne(WidgetRole::FontUI);
        });
    connect(&S, &Settings::fontSizeResultsChanged, this, [this, refreshOne](int pt) {
        m_globalSizeResults = pt; refreshOne(WidgetRole::FontResults);
        });
    connect(&S, &Settings::fontSizeSeparatorsChanged, this, [this, refreshOne](int pt) {
        m_globalSizeSeparators = pt; refreshOne(WidgetRole::FontSeparators);
        });
    connect(&S, &Settings::fontSizeInputChanged, this, [this, refreshOne](int pt) {
        m_globalSizeInput = pt; refreshOne(WidgetRole::FontInput);
        });
    connect(&S, &Settings::fontSizeSidebarChanged, this, [this, refreshOne](int pt) {
        m_globalSizeSidebar = pt; refreshOne(WidgetRole::FontSidebar);
        });

    connect(&S, &Settings::themeChanged, this, [this](const QString&) {
        m_globalAccent = Theme::accentColor();
        m_globalFamily = Theme::fontFamily();
        refreshAll();
        });
    connect(&S, &Settings::fontSizeSplashChanged, this, [this, refreshOne](int pt) {
        m_globalSizeSplash = pt; refreshOne(WidgetRole::FontSplash);
        });
    connect(&S, &Settings::visibilityLevelChanged, this, [this](VisibilityLevel level) {
        applyVisibilityLevel(level);
        });

    connect(&S, &Settings::settingsReset, this, [this]() {
        m_globalAccent = Theme::accentColor();
        m_globalFamily = Theme::fontFamily();
        m_globalSizeUI = Settings::instance().fontSizeUI();
        m_globalSizeResults = Settings::instance().fontSizeResults();
        m_globalSizeSeparators = Settings::instance().fontSizeSeparators();
        m_globalSizeInput = Settings::instance().fontSizeInput();
        m_globalSizeSidebar = Settings::instance().fontSizeSidebar();
        m_globalSizeSplash = Settings::instance().fontSizeSplash();
        m_globalLevel = Settings::instance().visibilityLevel();
        refreshAll();
        });
}

// Refreshes every widget carrying ANY font role — used when family/weight
// changes, since those apply across all five buckets at once.
void WidgetRegistry::refreshFontRoles() {
    static constexpr WidgetRole anyFont =
        WidgetRole::FontUI | WidgetRole::FontResults | WidgetRole::FontSeparators |
        WidgetRole::FontInput | WidgetRole::FontSidebar | WidgetRole::FontSplash;
    for (auto& e : m_entries)
        if (e.widget && hasRole(e.roles, anyFont))
            applyEntry(e);
}

// -- Registration --------------------------------------------------------------

int WidgetRegistry::add(QWidget* widget, WidgetRole roles, bool applyNow) {
    if (!widget) return -1;

    const int id = m_nextId++;
    Entry& e = m_entries[id];
    e.id = id;
    e.widget = widget;
    e.roles = roles;

    // Auto-deregister on destroy — no dangling entries, no dangling pointers
    connect(widget, &QObject::destroyed, this, [this, id]() {
        m_entries.remove(id);
        });

    if (applyNow) applyEntry(e);
    return id;
}

void WidgetRegistry::remove(int id) {
    m_entries.remove(id);
}

// -- Per-widget overrides ------------------------------------------------------

void WidgetRegistry::setStyle(int id, const WidgetStyle& style) {
    auto it = m_entries.find(id);
    if (it == m_entries.end()) return;
    Entry& e = it.value();
    // Merge — only fields that are set in the incoming style overwrite existing ones
    if (style.accentColor) e.override.accentColor = style.accentColor;
    if (style.stylesheet)  e.override.stylesheet = style.stylesheet;
    if (style.fontSize)    e.override.fontSize = style.fontSize;
    if (style.fontFamily)  e.override.fontFamily = style.fontFamily;
    if (style.visible)     e.override.visible = style.visible;
    e.hasOverride = true;
    applyEntry(e);
}

void WidgetRegistry::setAccentColor(int id, const QString& color) {
    setStyle(id, { .accentColor = color });
}
void WidgetRegistry::setStylesheet(int id, const QString& qss) {
    setStyle(id, { .stylesheet = qss });
}
void WidgetRegistry::setFontSize(int id, int pt) {
    setStyle(id, { .fontSize = pt });
}
void WidgetRegistry::setFontFamily(int id, const QString& family) {
    setStyle(id, { .fontFamily = family });
}
void WidgetRegistry::setVisible(int id, bool v) {
    setStyle(id, { .visible = v });
}

void WidgetRegistry::clearStyle(int id) {
    auto it = m_entries.find(id);
    if (it == m_entries.end()) return;
    it->override = {};
    it->hasOverride = false;
    applyEntry(it.value());
}

// -- Global refresh ------------------------------------------------------------

void WidgetRegistry::refreshAll() {

    for (auto& e : m_entries)
        if (e.widget) applyEntry(e);
}

void WidgetRegistry::refresh(int id) {
    auto it = m_entries.find(id);
    if (it != m_entries.end() && it->widget)
        applyEntry(it.value());
}

void WidgetRegistry::applyVisibilityLevel(VisibilityLevel level) {
    m_globalLevel = level;
    for (auto& e : m_entries) {
        if (!e.widget) continue;
        // Per-widget override always wins
        if (e.hasOverride && e.override.visible.has_value()) {
            e.widget->setVisible(*e.override.visible);
            continue;
        }
        if (hasRole(e.roles, WidgetRole::DeveloperOnly))
            e.widget->setVisible(level == VisibilityLevel::Developer);
        else if (hasRole(e.roles, WidgetRole::AdvancedPlus))
            e.widget->setVisible(level >= VisibilityLevel::Advanced);
        else if (hasRole(e.roles, WidgetRole::BasicOnly))
            e.widget->setVisible(level == VisibilityLevel::Basic);
    }
}

// -- applyEntry — core dispatch ------------------------------------------------

void WidgetRegistry::applyEntry(Entry& e) {
    if (!e.widget) return;

    // -- Visibility -----------------------------------------------------------
    if (e.hasOverride && e.override.visible.has_value()) {
        e.widget->setVisible(*e.override.visible);
    }
    else {
        if (hasRole(e.roles, WidgetRole::DeveloperOnly))
            e.widget->setVisible(m_globalLevel == VisibilityLevel::Developer);
        else if (hasRole(e.roles, WidgetRole::AdvancedPlus))
            e.widget->setVisible(m_globalLevel >= VisibilityLevel::Advanced);
        else if (hasRole(e.roles, WidgetRole::BasicOnly))
            e.widget->setVisible(m_globalLevel == VisibilityLevel::Basic);
    }

    // -- Font -------------------------------------------------------------------
    static constexpr WidgetRole anyFont =
        WidgetRole::FontUI | WidgetRole::FontResults | WidgetRole::FontSeparators |
        WidgetRole::FontInput | WidgetRole::FontSidebar | WidgetRole::FontSplash;
    if (hasRole(e.roles, anyFont))
        e.widget->setFont(buildFont(e));

    // -- Stylesheet -----------------------------------------------------------
    if (e.hasOverride && e.override.stylesheet.has_value()) {
        e.widget->setStyleSheet(*e.override.stylesheet);
    }
    else {
        const QString qss = buildStylesheet(e);
        if (!qss.isEmpty())
            e.widget->setStyleSheet(qss);
    }
}



// -- buildStylesheet -----------------------------------------------------------

QString WidgetRegistry::buildStylesheet(const Entry& e) const {
    const QString accent = (e.hasOverride && e.override.accentColor.has_value())
        ? *e.override.accentColor
        : m_globalAccent;
    const QString dim = Theme::dimAccent(); // always derived from current global

    QStringList parts;

    if (hasRole(e.roles, WidgetRole::AccentFill)) {
        parts << QString(
            "QPushButton{background:%1;color:#000;border:none;border-radius:6px;letter-spacing:1px;}"
            "QPushButton:hover{background:%2;}"
        ).arg(accent, dim);
    }
    if (hasRole(e.roles, WidgetRole::AccentBorder)) {
        parts << QString("border:1px solid %1;").arg(accent);
    }
    if (hasRole(e.roles, WidgetRole::AccentText)) {
        parts << QString("color:%1;background:transparent;").arg(accent);
    }
    if (hasRole(e.roles, WidgetRole::Text)) {
        parts << QString("color:%1;background:transparent;").arg(Theme::TEXT);
    }
    if (hasRole(e.roles, WidgetRole::Muted)) {
        parts << QString("color:%1;background:transparent;").arg(Theme::MUTED);
    }
    if (hasRole(e.roles, WidgetRole::Surface)) {
        parts << QString("background:%1;").arg(Theme::SURFACE);
    }
    if (hasRole(e.roles, WidgetRole::Card)) {
        parts << QString("background:%1;").arg(Theme::CARD);
    }

    return parts.join('\n');
}

// -- buildFont -----------------------------------------------------------------

QFont WidgetRegistry::buildFont(const Entry& e) const {
    const QString family = (e.hasOverride && e.override.fontFamily.has_value())
        ? *e.override.fontFamily : m_globalFamily;

    // Precedence when a widget somehow carries multiple font roles (shouldn't
    // normally happen, but Results > Separators > Input > Sidebar > UI keeps
    // behavior deterministic rather than undefined).
    int pt;
    if (e.hasOverride && e.override.fontSize.has_value())
        pt = *e.override.fontSize;
    else if (hasRole(e.roles, WidgetRole::FontResults))    pt = m_globalSizeResults;
    else if (hasRole(e.roles, WidgetRole::FontSeparators)) pt = m_globalSizeSeparators;
    else if (hasRole(e.roles, WidgetRole::FontInput))      pt = m_globalSizeInput;
    else if (hasRole(e.roles, WidgetRole::FontSidebar))    pt = m_globalSizeSidebar;
    else if (hasRole(e.roles, WidgetRole::FontSplash))     pt = m_globalSizeSplash;
    else                                                    pt = m_globalSizeUI;

    QFont f(family);
    // Never pass a non-positive size to Qt. This should be unreachable now that
    // all five fontSize* keys are registered in SettingsDef (they weren't, which
    // made Settings::get() return an invalid QVariant, .toInt() give 0, and the
    // constructor above overwrite its own defaults with zeroes). But the failure
    // was silent and lasted months: two "QFont::setPointSize: Point size <= 0"
    // warnings per result line, five CacheOverflowExceptions at startup, and
    // every label quietly falling back to a system font. A missing registration
    // is a one-line mistake anyone can make again -- this makes the consequence
    // a slightly-wrong font size instead of a broken one.
    f.setPointSize(pt > 0 ? pt : 9);
    f.setStyleHint(QFont::Monospace);

    static const QHash<QString, QFont::Weight> weightMap = {
        {"Thin", QFont::Thin}, {"Light", QFont::Light}, {"Normal", QFont::Normal},
        {"Medium", QFont::Medium}, {"Bold", QFont::Bold}, {"Extra Bold", QFont::ExtraBold}
    };
    f.setWeight(weightMap.value(m_globalWeight, QFont::Normal));
    return f;
}