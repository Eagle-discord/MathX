#pragma once
#include <QString>
#include <QVariant>
#include <QList>
#include "Settings.h"

// -- ControlType ---------------------------------------------------------------
// Drives which widget the settings UI renders for this setting.
enum class ControlType {
    Toggle,       // Boolean — red/green push button
    Dropdown,     // Enum — styled dropdown
    Slider,       // Int — adaptive step slider + inline edit
    ColorPicker,  // Color — hex/rgb/hsv input + picker popup
    FontInput,    // Font — typed input with autofill suggestions
    TextInput,    // String — plain text input
};

// -- CategoryId ----------------------------------------------------------------
enum class CategoryId {
    Appearance,
    Display,
    Behavior,
    System,
};

// -- SubcategoryId -------------------------------------------------------------
enum class SubcategoryId {
    // Appearance
    Typography,
    Colors,
    Theme,

    // Display
    Progress,
    Results,
    GeometryViewer,

    // Behavior
    Threading,
    Computation,
    Memory,

    // System
    Data,
    About,
};

// -- SliderRange ---------------------------------------------------------------
// Only meaningful for Slider controls.
struct SliderRange {
    double min = 0.0;
    double max = 100.0;
    double step = 1.0;   // base step — adapts as the user drags further
};

// -- SettingDef ----------------------------------------------------------------
// The complete description of a single setting.
// Adding a new setting means adding one entry to the registry in SettingsDef.cpp.
// Nothing else needs to change.
struct SettingDef {
    // -- Identity --------------------------------------------------------------
    QString         key;              // persistence key e.g. "appearance/typography/fontSize"
    // matches QSettings path exactly

// -- Classification --------------------------------------------------------
    CategoryId      category;         // which master category this belongs to
    SubcategoryId   subcategory;      // which subcategory within that category

    // -- Value -----------------------------------------------------------------
    QVariant        defaultValue;     // value before the user touches anything
    ControlType     control;          // which widget renders this setting
    QStringList     options;          // only for Dropdown — list of valid choices
    SliderRange     range;            // only for Slider — min, max, base step

    // -- Labels ----------------------------------------------------------------
    // Two label variants — Basic uses plain English, Advanced/Developer uses
    // the more technical label. If identical, set both to the same string.
    QString         labelBasic;       // "Text size"
    QString         labelAdvanced;    // "Font point size"

    // -- Descriptions ---------------------------------------------------------
    // Basic    — always visible, plain English, one sentence
    // Advanced — shown on hover, slightly more technical
    // Developer metadata is auto-generated from key + type + range + applyMode
    QString         descBasic;        // "Changes how large the text appears"
    QString         descAdvanced;     // "Adjusts the interface font point size (pt)"

    // -- Apply pipeline --------------------------------------------------------
    ApplyMode       applyMode;        // Immediate | Staged | Deferred

    // Shown in the apply animation sequence
    QString         applyingLabel;    // "Updating text size across the interface"
    QString         appliedLabel;     // "Text size updated"

    // -- Visibility ------------------------------------------------------------
    // Minimum visibility level required to see this setting.
    // Basic settings are always shown.
    // Advanced settings hidden until user sets level to Advanced.
    // Developer settings hidden until Developer level.
    VisibilityLevel minLevel;

    // -- Search aliases --------------------------------------------------------
    // Extra search terms that should match this setting even though they
    // don't appear in its labels/description. E.g. "Text size" might also
    // be found by searching "font size" or "point size".
    // Optional — defaults to empty, append via initializer list as needed.
    QStringList     aliases = {};
};

// -- CategoryDef ---------------------------------------------------------------
// Describes a master category — drives the category card rendering.
struct CategoryDef {
    CategoryId  id;
    QString     label;          // "Appearance"
    QString     accentColor;    // hex color string matching Theme constants
    QString     iconName;       // logical icon name — mapped to an emoji glyph in CategoryCard
    QString     description;    // one line shown on the card in C_MUTED
    QString     hint;           // short purpose statement shown below description
};

// -- SubcategoryDef ------------------------------------------------------------
// Describes a subcategory — drives the subcategory card rendering.
struct SubcategoryDef {
    SubcategoryId   id;
    CategoryId      parentCategory; // which master category owns this
    QString         label;          // "Typography"
    QString         iconName;       // logical icon name
    QString         description;    // one line shown on the subcategory card
};

// -- Registries ----------------------------------------------------------------
// Defined in SettingsDef.cpp — the single source of truth for everything.
// All other systems read from these; nothing is hardcoded elsewhere.

// All master categories, in display order
const QList<CategoryDef>& allCategories();

// All subcategories, in display order
const QList<SubcategoryDef>& allSubcategories();

// All settings, in display order within each subcategory
const QList<SettingDef>& allSettings();

// -- Lookup helpers ------------------------------------------------------------
// Find a single SettingDef by key. Returns nullptr if not found.
const SettingDef* findSetting(const QString& key);

// All settings belonging to a subcategory, filtered by visibility level
QList<SettingDef> settingsFor(SubcategoryId sub, VisibilityLevel level);

// All subcategories belonging to a category
QList<SubcategoryDef> subcategoriesFor(CategoryId cat);

// Search across all settings visible at the given level.
// Matches against labelBasic, labelAdvanced, descBasic, and key.
QList<SettingDef> searchSettings(const QString& query, VisibilityLevel level);