#include "SettingsDef.h"
#include "../constants/Theme.h"

// -- Category registry ---------------------------------------------------------
const QList<CategoryDef>& allCategories() {
    static const QList<CategoryDef> list = {
        {
            CategoryId::Appearance,
            "Appearance",
            Theme::WARN,        // amber — warmth, creativity
            "brush",
            "Fonts, colors, and visual style",
            "Customize the look and feel"
        },
        {
            CategoryId::Display,
            "Display",
            Theme::INFO,        // blue — screens, clarity
            "monitor",
            "How results and output are presented",
            "Control how information is shown"
        },
        {
            CategoryId::Behavior,
            "Behavior",
            Theme::ACCENT,      // green — logic, the app's primary identity
            "gear",
            "How the app is programmed internally",
            "Configure calculation logic and automation"
        },
        {
            CategoryId::System,
            "System",
            Theme::PURPLE,      // purple — infrastructure, depth
            "server",
            "App-level data and information",
            "Manage application data and diagnostics"
        },
    };
    return list;
}

// -- Subcategory registry ------------------------------------------------------
const QList<SubcategoryDef>& allSubcategories() {
    static const QList<SubcategoryDef> list = {
        // -- Appearance --------------------------------------------------------
        {
            SubcategoryId::Typography,
            CategoryId::Appearance,
            "Typography",
            "text",
            "Font family, size, and weight"
        },
        {
            SubcategoryId::Colors,
            CategoryId::Appearance,
            "Colors",
            "palette",
            "Individual color overrides for UI elements"
        },
        {
            SubcategoryId::Theme,
            CategoryId::Appearance,
            "Theme",
            "swatches",
            "Preset themes and custom theme editor"
        },

        // -- Display -----------------------------------------------------------
        {
            SubcategoryId::Progress,
            CategoryId::Display,
            "Progress",
            "activity",
            "How computation progress is shown"
        },
        {
            SubcategoryId::Results,
            CategoryId::Display,
            "Results",
            "terminal",
            "How results are formatted and displayed"
        },
        {
            SubcategoryId::GeometryViewer,
            CategoryId::Display,
            "Geometry Viewer",
            "cube",
            "3D viewer display settings"
        },

        // -- Behavior ----------------------------------------------------------
        {
            SubcategoryId::Threading,
            CategoryId::Behavior,
            "Threading",
            "cpu",
            "Worker thread architecture and configuration"
        },
        {
            SubcategoryId::Computation,
            CategoryId::Behavior,
            "Computation",
            "math",
            "Precision, angle units, and numeric thresholds"
        },
        {
            SubcategoryId::Memory,
            CategoryId::Behavior,
            "Memory",
            "database",
            "History size and cache behavior"
        },

        // -- System ------------------------------------------------------------
        {
            SubcategoryId::Data,
            CategoryId::System,
            "Data",
            "archive",
            "Reset, export, and import settings"
        },
        {
            SubcategoryId::About,
            CategoryId::System,
            "About",
            "info-circle",
            "Version, build info, and licenses"
        },
    };
    return list;
}

// -- Setting registry ----------------------------------------------------------
// This is the single source of truth for every setting in MathX.
// To add a new setting, add one entry here. Nothing else needs to change.
//
// Key path convention: "category/subcategory/settingName"
// ApplyMode guide:
//   Immediate — safe at any time (visual, display)
//   Staged    — structural changes, apply on next Idle (threading, thresholds)
//   Deferred  — math-affecting changes, apply after current op (angle unit, precision)

const QList<SettingDef>& allSettings() {
    static const QList<SettingDef> list = {

        // ════════════════════════════════════════════════════════════════════
        // APPEARANCE / TYPOGRAPHY
        // ════════════════════════════════════════════════════════════════════
        {
            "appearance/typography/fontSize",
            CategoryId::Appearance, SubcategoryId::Typography,
            10, ControlType::Slider, {}, {8.0, 16.0, 1.0},
            "Text size", "Font point size",
            "Changes how large the text appears",
            "Adjusts the interface font point size (pt)",
            ApplyMode::Immediate,
            "Updating text size across the interface",
            "Text size updated",
            VisibilityLevel::Basic,
            {"Font size", "Point size", "Text scale"}
        },
        {
            "appearance/typography/fontFamily",
            CategoryId::Appearance, SubcategoryId::Typography,
            "JetBrains Mono", ControlType::FontInput, {}, {},
            "Font", "Font family",
            "Changes the text style used throughout the app",
            "Sets the monospace font family for all interface text",
            ApplyMode::Immediate,
            "Applying new font across the interface",
            "Font updated",
            VisibilityLevel::Basic,
            {"Typeface", "Font family"}
        },
        {
            "appearance/typography/fontWeight",
            CategoryId::Appearance, SubcategoryId::Typography,
            "Normal", ControlType::Dropdown,
            {"Thin", "Light", "Normal", "Medium", "Bold", "Extra Bold"}, {},
            "Text weight", "Font weight",
            "Changes how thick or thin the text looks",
            "Sets the font weight for interface text",
            ApplyMode::Immediate,
            "Updating font weight",
            "Font weight updated",
            VisibilityLevel::Advanced
        },

        // ════════════════════════════════════════════════════════════════════
        // APPEARANCE / COLORS
        // ════════════════════════════════════════════════════════════════════
        {
            "appearance/colors/accentColor",
            CategoryId::Appearance, SubcategoryId::Colors,
            "#00e87a", ControlType::ColorPicker, {}, {},
            "Highlight color", "Accent color",
            "The main color used for buttons, highlights, and results",
            "Primary accent color applied across interactive elements",
            ApplyMode::Immediate,
            "Updating accent color across the interface",
            "Accent color updated",
            VisibilityLevel::Basic,
            {"Theme color", "Primary color"}
        },
        {
            "appearance/colors/backgroundColor",
            CategoryId::Appearance, SubcategoryId::Colors,
            "#0b0d0c", ControlType::ColorPicker, {}, {},
            "Background color", "Background color",
            "The main background color of the app",
            "Application background (C_BG)",
            ApplyMode::Immediate,
            "Updating background color",
            "Background color updated",
            VisibilityLevel::Advanced
        },
        {
            "appearance/colors/surfaceColor",
            CategoryId::Appearance, SubcategoryId::Colors,
            "#111413", ControlType::ColorPicker, {}, {},
            "Panel color", "Surface color",
            "The color of panels and cards inside the app",
            "Surface color for elevated UI elements (C_SURFACE)",
            ApplyMode::Immediate,
            "Updating surface color",
            "Surface color updated",
            VisibilityLevel::Advanced
        },
        {
            "appearance/colors/borderColor",
            CategoryId::Appearance, SubcategoryId::Colors,
            "#252927", ControlType::ColorPicker, {}, {},
            "Border color", "Border color",
            "The color of lines that separate sections",
            "Border color for UI element outlines (C_BORDER)",
            ApplyMode::Immediate,
            "Updating border color",
            "Border color updated",
            VisibilityLevel::Advanced
        },
        {
            "appearance/colors/textColor",
            CategoryId::Appearance, SubcategoryId::Colors,
            "#ddeae0", ControlType::ColorPicker, {}, {},
            "Text color", "Primary text color",
            "The main color of all text in the app",
            "Primary text color (C_TEXT)",
            ApplyMode::Immediate,
            "Updating text color",
            "Text color updated",
            VisibilityLevel::Advanced
        },
        {
            "appearance/colors/mutedColor",
            CategoryId::Appearance, SubcategoryId::Colors,
            "#5a6b5f", ControlType::ColorPicker, {}, {},
            "Subtle text color", "Muted text color",
            "The color of less important text like labels and hints",
            "Muted text color for secondary labels (C_MUTED)",
            ApplyMode::Immediate,
            "Updating muted color",
            "Muted color updated",
            VisibilityLevel::Advanced
        },

        // ════════════════════════════════════════════════════════════════════
        // APPEARANCE / THEME
        // ════════════════════════════════════════════════════════════════════
        {
            "appearance/theme/preset",
            CategoryId::Appearance, SubcategoryId::Theme,
            "Dark", ControlType::Dropdown,
            {"Dark", "Light", "Custom"}, {},
            "Theme", "Theme preset",
            "Choose a preset look for the app",
            "Applies a preset color palette across all UI elements",
            ApplyMode::Immediate,
            "Applying theme",
            "Theme applied",
            VisibilityLevel::Basic
        },

        // ════════════════════════════════════════════════════════════════════
        // DISPLAY / PROGRESS
        // ════════════════════════════════════════════════════════════════════
        {
            "display/progress/style",
            CategoryId::Display, SubcategoryId::Progress,
            "Bar", ControlType::Dropdown,
            {"Bar", "Text", "None"}, {},
            "Progress display", "Progress display style",
            "How computation progress is shown while calculating",
            "Controls the progress indicator style during heavy operations",
            ApplyMode::Immediate,
            "Updating progress display style",
            "Progress style updated",
            VisibilityLevel::Basic
        },
        {
            "display/progress/showForHeavyOnly",
            CategoryId::Display, SubcategoryId::Progress,
            true, ControlType::Toggle, {}, {},
            "Only show for heavy math", "Show progress for heavy operations only",
            "Hides the progress bar for simple calculations",
            "Suppresses progress display for non-intensive operations",
            ApplyMode::Immediate,
            "Updating progress visibility rules",
            "Progress visibility updated",
            VisibilityLevel::Basic
        },

        // ════════════════════════════════════════════════════════════════════
        // DISPLAY / RESULTS
        // ════════════════════════════════════════════════════════════════════
        {
            "display/results/truncationLimit",
            CategoryId::Display, SubcategoryId::Results,
            500, ControlType::Slider, {}, {100.0, 10000.0, 100.0},
            "Result length limit", "Result truncation limit",
            "How many characters to show before cutting off long results",
            "Maximum characters shown inline before truncation footer appears",
            ApplyMode::Immediate,
            "Updating truncation limit",
            "Truncation limit updated",
            VisibilityLevel::Basic
        },
        {
            "display/results/showSeparator",
            CategoryId::Display, SubcategoryId::Results,
            true, ControlType::Toggle, {}, {},
            "Show line between results", "Show result separator",
            "Draws a line between each calculation to make them easier to read",
            "Renders a horizontal separator between result entries",
            ApplyMode::Immediate,
            "Updating separator visibility",
            "Separator setting updated",
            VisibilityLevel::Basic
        },

        // ════════════════════════════════════════════════════════════════════
        // DISPLAY / GEOMETRY VIEWER
        // ════════════════════════════════════════════════════════════════════
        {
            "display/geometry/autoRotate",
            CategoryId::Display, SubcategoryId::GeometryViewer,
            true, ControlType::Toggle, {}, {},
            "Spin shapes automatically", "Auto-rotate shapes",
            "Shapes slowly spin when first opened in the 3D viewer",
            "Enables automatic rotation animation on shape load",
            ApplyMode::Immediate,
            "Updating auto-rotate setting",
            "Auto-rotate updated",
            VisibilityLevel::Basic
        },
        {
            "display/geometry/defaultShapeColor",
            CategoryId::Display, SubcategoryId::GeometryViewer,
            "#ffffff", ControlType::ColorPicker, {}, {},
            "Default shape color", "Default shape color",
            "The color shapes start with when opened in the 3D viewer",
            "Default wireframe color for shapes in the OpenGL renderer",
            ApplyMode::Immediate,
            "Updating default shape color",
            "Shape color updated",
            VisibilityLevel::Basic
        },
        {
            "display/geometry/showPropertyLabels",
            CategoryId::Display, SubcategoryId::GeometryViewer,
            false, ControlType::Toggle, {}, {},
            "Show measurements on shapes", "Show property labels",
            "Displays radius, diameter, and other measurements on 3D shapes",
            "Renders property overlay labels in the OpenGL viewport",
            ApplyMode::Immediate,
            "Updating property label visibility",
            "Property labels updated",
            VisibilityLevel::Basic
        },

        // ════════════════════════════════════════════════════════════════════
        // BEHAVIOR / THREADING
        // ════════════════════════════════════════════════════════════════════
        {
            "behavior/threading/splitThreads",
            CategoryId::Behavior, SubcategoryId::Threading,
            true, ControlType::Toggle, {}, {},
            "Separate math from UI", "Enable split UI/math threads",
            "Keeps the app responsive while doing heavy calculations",
            "Runs computation on a dedicated worker thread separate from the UI thread",
            ApplyMode::Staged,
            "Reconfiguring thread architecture",
            "Thread architecture updated",
            VisibilityLevel::Advanced
        },
        {
            "behavior/threading/workerPriority",
            CategoryId::Behavior, SubcategoryId::Threading,
            "Normal", ControlType::Dropdown,
            {"Low", "Normal", "High"}, {},
            "Math thread priority", "Worker thread priority",
            "How much of the CPU the math thread gets relative to other apps",
            "Sets QThread priority for the PersistentWorker thread",
            ApplyMode::Staged,
            "Updating worker thread priority",
            "Thread priority updated",
            VisibilityLevel::Developer
        },

        // ════════════════════════════════════════════════════════════════════
        // BEHAVIOR / COMPUTATION
        // ════════════════════════════════════════════════════════════════════
        {
            "behavior/computation/angleUnit",
            CategoryId::Behavior, SubcategoryId::Computation,
            "Degrees", ControlType::Dropdown,
            {"Degrees", "Radians", "Gradians"}, {},
            "Angle unit", "Angle unit",
            "The unit used for angles in trig functions like sin and cos",
            "Unit passed to trigonometric functions in MathEngine",
            ApplyMode::Deferred,
            "Switching angle unit",
            "Angle unit updated",
            VisibilityLevel::Basic
        },
        {
            "behavior/computation/decimalPlaces",
            CategoryId::Behavior, SubcategoryId::Computation,
            10, ControlType::Slider, {}, {1.0, 20.0, 1.0},
            "Decimal places", "Decimal precision",
            "How many decimal places to show in results",
            "Precision passed to result formatting in MathEngine",
            ApplyMode::Deferred,
            "Updating decimal precision",
            "Precision updated",
            VisibilityLevel::Basic,
            {"Precision", "Rounding"}
        },
        {
            "behavior/computation/bigNumThreshold",
            CategoryId::Behavior, SubcategoryId::Computation,
            50000, ControlType::Slider, {}, {1000.0, 1000000.0, 1000.0},
            "Big number threshold", "Big number async threshold",
            "How large a number needs to be before it uses the background calculator",
            "n value above which factorial/pow routes to PersistentWorker async path",
            ApplyMode::Staged,
            "Updating big number threshold",
            "Threshold updated",
            VisibilityLevel::Advanced
        },
        {
            "behavior/computation/streamChunkSize",
            CategoryId::Behavior, SubcategoryId::Computation,
            500, ControlType::Slider, {}, {100.0, 10000.0, 100.0},
            "Streaming chunk size", "Result streaming chunk size",
            "How many digits are sent at a time when streaming a very large result",
            "Character count per chunkReady signal emission in BigNum::bigFactorial",
            ApplyMode::Staged,
            "Updating stream chunk size",
            "Chunk size updated",
            VisibilityLevel::Developer
        },

        // ════════════════════════════════════════════════════════════════════
        // BEHAVIOR / MEMORY
        // ════════════════════════════════════════════════════════════════════
        {
            "behavior/memory/historySize",
            CategoryId::Behavior, SubcategoryId::Memory,
            100, ControlType::Slider, {}, {10.0, 500.0, 10.0},
            "History size", "History size",
            "How many past calculations to remember",
            "Maximum entries retained in the session history navigator",
            ApplyMode::Immediate,
            "Updating history size",
            "History size updated",
            VisibilityLevel::Basic
        },
        {
            "behavior/memory/confirmClear",
            CategoryId::Behavior, SubcategoryId::Memory,
            true, ControlType::Toggle, {}, {},
            "Ask before clearing history", "Confirm before clear",
            "Shows a confirmation before wiping all calculations",
            "Requires confirmation dialog before onClear() executes",
            ApplyMode::Immediate,
            "Updating clear confirmation setting",
            "Confirmation setting updated",
            VisibilityLevel::Basic
        },
        {
            "behavior/memory/clearOnExit",
            CategoryId::Behavior, SubcategoryId::Memory,
            false, ControlType::Toggle, {}, {},
            "Clear history on close", "Clear history on exit",
            "Automatically wipes all calculations when the app is closed",
            "Calls onClear() in closeEvent() before shutdown",
            ApplyMode::Immediate,
            "Updating exit behavior",
            "Exit behavior updated",
            VisibilityLevel::Advanced
        },

        // ════════════════════════════════════════════════════════════════════
        // SYSTEM / DATA
        // ════════════════════════════════════════════════════════════════════
        {
            "system/data/exportSettings",
            CategoryId::System, SubcategoryId::Data,
            false, ControlType::Toggle, {}, {},
            "Export settings to file", "Export settings",
            "Save all your settings to a file you can share or back up",
            "Serialises QSettings to JSON via QJsonDocument",
            ApplyMode::Immediate,
            "Exporting settings",
            "Settings exported",
            VisibilityLevel::Basic
        },
        {
            "system/data/importSettings",
            CategoryId::System, SubcategoryId::Data,
            false, ControlType::Toggle, {}, {},
            "Import settings from file", "Import settings",
            "Load settings from a file exported by MathX",
            "Deserialises JSON into QSettings, validates keys against SettingsDef",
            ApplyMode::Immediate,
            "Importing settings",
            "Settings imported",
            VisibilityLevel::Basic
        },
        {
            "system/data/resetAll",
            CategoryId::System, SubcategoryId::Data,
            false, ControlType::Toggle, {}, {},
            "Reset everything to defaults", "Reset all settings",
            "Wipes all your settings and starts fresh",
            "Calls Settings::instance().resetAll() — irreversible without export",
            ApplyMode::Immediate,
            "Resetting all settings to defaults",
            "Settings reset",
            VisibilityLevel::Basic
        },
    };
    return list;
}

// -- Lookup helpers ------------------------------------------------------------

const SettingDef* findSetting(const QString& key) {
    for (const SettingDef& def : allSettings()) {
        if (def.key == key) return &def;
    }
    return nullptr;
}

QList<SettingDef> settingsFor(SubcategoryId sub, VisibilityLevel level) {
    QList<SettingDef> result;
    for (const SettingDef& def : allSettings()) {
        if (def.subcategory == sub && def.minLevel <= level)
            result.append(def);
    }
    return result;
}

QList<SubcategoryDef> subcategoriesFor(CategoryId cat) {
    QList<SubcategoryDef> result;
    for (const SubcategoryDef& def : allSubcategories()) {
        if (def.parentCategory == cat)
            result.append(def);
    }
    return result;
}

QList<SettingDef> searchSettings(const QString& query, VisibilityLevel level) {
    if (query.trimmed().isEmpty()) return {};
    QList<SettingDef> result;
    const QString q = query.toLower();
    for (const SettingDef& def : allSettings()) {
        if (def.minLevel > level) continue;

        bool aliasMatch = false;
        for (const QString& alias : def.aliases) {
            if (alias.toLower().contains(q)) { aliasMatch = true; break; }
        }

        if (def.labelBasic.toLower().contains(q) ||
            def.labelAdvanced.toLower().contains(q) ||
            def.descBasic.toLower().contains(q) ||
            def.key.toLower().contains(q) ||
            aliasMatch)
        {
            result.append(def);
        }
    }
    return result;
}