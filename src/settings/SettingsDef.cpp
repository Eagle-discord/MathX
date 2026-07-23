#include "SettingsDef.h"
#include "../constants/Theme.h"

// -- Category registry ---------------------------------------------------------
const QList<CategoryDef>& allCategories() {
    static const QList<CategoryDef> list = []() {
        QList<CategoryDef> l;
        l.reserve(4);

        CategoryDef appearance;
        appearance.id = CategoryId::Appearance;
        appearance.label = "Appearance";
        appearance.accentColor = Theme::WARN;
        appearance.iconName = "brush";
        appearance.description = "Fonts, colors, and visual style";
        appearance.hint = "Customize the look and feel";
        l.append(appearance);

        CategoryDef display;
        display.id = CategoryId::Display;
        display.label = "Display";
        display.accentColor = Theme::INFO;
        display.iconName = "monitor";
        display.description = "How results and output are presented";
        display.hint = "Control how information is shown";
        l.append(display);

        CategoryDef behavior;
        behavior.id = CategoryId::Behavior;
        behavior.label = "Behavior";
        behavior.accentColor = Theme::ACCENT();
        behavior.iconName = "gear";
        behavior.description = "How the app is programmed internally";
        behavior.hint = "Configure calculation logic and automation";
        l.append(behavior);

        CategoryDef system;
        system.id = CategoryId::System;
        system.label = "System";
        system.accentColor = Theme::PURPLE;
        system.iconName = "server";
        system.description = "App-level data and information";
        system.hint = "Manage application data and diagnostics";
        l.append(system);

        return l;
        }();
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
        {
            SubcategoryId::Animations,
            CategoryId::Behavior,
            "Animations",
            "sparkle",
            "Animation preferences for the settings page"
        },
        {
            SubcategoryId::Developer,
            CategoryId::Behavior,
            "Developer",
            "code",
            "Advanced developer-only tools"
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
//   Immediate - safe at any time (visual, display)
//   Staged    - structural changes, apply on next Idle (threading, thresholds)
//   Deferred  - math-affecting changes, apply after current op (angle unit, precision)
//
// NOTE on `affects`: this feeds the pending-queue and apply-animation labels
// ("{labelBasic} - {affects}"). Every setting that can enter the pending
// queue (i.e. every non-Action control) has one filled in below, so no
// label ever falls back to the bare, sometimes-ambiguous labelBasic alone.
// Action-type settings (one-shot buttons) never enter the pending queue,
// so `affects` is intentionally left blank for them.

const QList<SettingDef>& allSettings() {
    static const QList<SettingDef> list = {

        // --------------------------------------------------------------------
        // APPEARANCE / TYPOGRAPHY
        // --------------------------------------------------------------------
        /*{
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
            {"Font size", "Point size", "Text scale"},
            "All interface text"
        },*/
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
            {"Typeface", "Font family"},
            "All interface text"
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
            VisibilityLevel::Advanced,
            {},
            "All interface text"
        },
        {
        "appearance/typography/fontSizeSplash",
        CategoryId::Appearance, SubcategoryId::Typography,
        9, ControlType::Slider, {}, {7.0, 20.0, 1.0},
        "Splash text size", "Splash screen font size",
        "Changes the size of the welcome title, instructions, and examples shown on launch",
        "Point size applied to the terminal splash screen content (title, instructions, sidebar examples)",
        ApplyMode::Immediate,
        "Updating splash text size",
        "Splash text size updated",
        VisibilityLevel::Advanced,
        {"Font size", "Welcome text", "Splash screen"},
        "Splash screen",
        "pt"
        },
        // --------------------------------------------------------------------
        // The five below were READ by Settings.cpp but never registered here.
        // get() on an unregistered key returns an invalid QVariant, and
        // QVariant().toInt() is 0 -- so WidgetRegistry's constructor overwrote
        // its own sane defaults (10/9/9/10/9) with zeroes and then called
        // setPointSize(0) on every tracked widget for the rest of the session.
        // That produced the "QFont::setPointSize: Point size <= 0 (0)" storm --
        // two per result line, forever -- plus five CacheOverflowExceptions at
        // startup, and made every label silently fall back to a system font
        // instead of the one main.cpp loads.
        //
        // The defaults here MUST match WidgetRegistry.h's in-class initialisers.
        // Registered alongside fontSizeSplash because they are the same setting
        // for different surfaces; splash was registered and the others were not,
        // which is exactly why only splash worked.
        // --------------------------------------------------------------------
        {
        "appearance/typography/fontSizeUI",
        CategoryId::Appearance, SubcategoryId::Typography,
        10, ControlType::Slider, {}, {7.0, 20.0, 1.0},
        "UI text size", "Interface font size",
        "Changes the size of buttons, labels, and panel text",
        "Point size applied to widgets registered with WidgetRole::FontUI",
        ApplyMode::Immediate,
        "Updating UI text size",
        "UI text size updated",
        VisibilityLevel::Advanced,
        {"Font size", "Buttons", "Interface"},
        "Interface",
        "pt"
        },
        {
        "appearance/typography/fontSizeResults",
        CategoryId::Appearance, SubcategoryId::Typography,
        9, ControlType::Slider, {}, {7.0, 20.0, 1.0},
        "Result text size", "Result font size",
        "Changes the size of calculation results in the terminal",
        "Point size applied to widgets registered with WidgetRole::FontResults",
        ApplyMode::Immediate,
        "Updating result text size",
        "Result text size updated",
        VisibilityLevel::Advanced,
        {"Font size", "Results", "Output"},
        "Results",
        "pt"
        },
        {
        "appearance/typography/fontSizeSeparators",
        CategoryId::Appearance, SubcategoryId::Typography,
        9, ControlType::Slider, {}, {7.0, 20.0, 1.0},
        "Separator text size", "Separator font size",
        "Changes the size of timing lines and separators between results",
        "Point size applied to widgets registered with WidgetRole::FontSeparators",
        ApplyMode::Immediate,
        "Updating separator text size",
        "Separator text size updated",
        VisibilityLevel::Advanced,
        {"Font size", "Separators", "Timing"},
        "Separators",
        "pt"
        },
        {
        "appearance/typography/fontSizeInput",
        CategoryId::Appearance, SubcategoryId::Typography,
        10, ControlType::Slider, {}, {7.0, 20.0, 1.0},
        "Input text size", "Input font size",
        "Changes the size of text in the expression input bar",
        "Point size applied to widgets registered with WidgetRole::FontInput",
        ApplyMode::Immediate,
        "Updating input text size",
        "Input text size updated",
        VisibilityLevel::Advanced,
        {"Font size", "Input", "Prompt"},
        "Input bar",
        "pt"
        },
        {
        "appearance/typography/fontSizeSidebar",
        CategoryId::Appearance, SubcategoryId::Typography,
        9, ControlType::Slider, {}, {7.0, 20.0, 1.0},
        "Sidebar text size", "Sidebar font size",
        "Changes the size of the quick reference and session panel text",
        "Point size applied to widgets registered with WidgetRole::FontSidebar",
        ApplyMode::Immediate,
        "Updating sidebar text size",
        "Sidebar text size updated",
        VisibilityLevel::Advanced,
        {"Font size", "Sidebar", "Reference"},
        "Sidebar",
        "pt"
        },
        // --------------------------------------------------------------------
        // APPEARANCE / COLORS
        // --------------------------------------------------------------------
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
            {"Theme color", "Primary color"},
            "Buttons & highlights"
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
            VisibilityLevel::Advanced,
            {},
            "Window background"
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
            VisibilityLevel::Advanced,
            {},
            "Panels & cards"
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
            VisibilityLevel::Advanced,
            {},
            "Panel outlines"
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
            VisibilityLevel::Advanced,
            {},
            "Primary text"
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
            VisibilityLevel::Advanced,
            {},
            "Labels & hints"
        },

        // --------------------------------------------------------------------
        // APPEARANCE / THEME
        // --------------------------------------------------------------------
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
            VisibilityLevel::Basic,
            {},
            "Entire color palette"
        },

        // --------------------------------------------------------------------
        // DISPLAY / PROGRESS
        // --------------------------------------------------------------------
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
            VisibilityLevel::Basic,
            {},
            "Progress indicator"
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
            VisibilityLevel::Basic,
            {},
            "Progress indicator"
        },

        // --------------------------------------------------------------------
        // DISPLAY / RESULTS
        // --------------------------------------------------------------------
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
            VisibilityLevel::Basic,
            {},
            "Result output"
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
            VisibilityLevel::Basic,
            {},
            "Result output"
        },

        // --------------------------------------------------------------------
        // DISPLAY / GEOMETRY VIEWER
        // --------------------------------------------------------------------
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
            VisibilityLevel::Basic,
            {},
            "3D shape viewer"
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
            VisibilityLevel::Basic,
            {},
            "3D shape viewer"
        },

        // --------------------------------------------------------------------
        // BEHAVIOR / THREADING
        // --------------------------------------------------------------------
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
            VisibilityLevel::Advanced,
            {},
            "Calculation engine"
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
            VisibilityLevel::Developer,
            {},
            "Calculation engine"
        },

        // --------------------------------------------------------------------
        // BEHAVIOR / COMPUTATION
        // --------------------------------------------------------------------
        {
            "behavior/results/numberNames",
            CategoryId::Behavior, SubcategoryId::Computation,
            "Number name", ControlType::Dropdown,
            {"Number name", "Full number", "Off"}, {},
            "Left-click a result", "Result click",
            "What left-clicking a numeric result does: name its magnitude in "
            "words (fifty, 1.35 quintillion), expand a collapsed value to all its "
            "digits, or nothing",
            "Chooses the left-click action wired onto numeric result labels",
            ApplyMode::Immediate,
            "Updating result click action",
            "Result click action updated",
            VisibilityLevel::Basic,
            {},
            "Result lines"
        },
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
            VisibilityLevel::Basic,
            {},
            "Trig functions"
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
            {"Precision", "Rounding"},
            "Result formatting"
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
            VisibilityLevel::Advanced,
            {},
            "Big number calculations"
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
            VisibilityLevel::Developer,
            {},
            "Big number calculations"
        },

        // --------------------------------------------------------------------
        // BEHAVIOR / MEMORY
        // --------------------------------------------------------------------
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
            VisibilityLevel::Basic,
            {},
            "Session history"
        },
        {
            "appearance/results/groupDigits",
            CategoryId::Appearance, SubcategoryId::Typography,
            false, ControlType::Toggle, {}, {},
            "Group digits in results", "Digit grouping",
            "Adds thousands separators to numbers in results (1,234,567)",
            "NumberFormat::groupNumbers() is bypassed when off",
            ApplyMode::Immediate,
            "Updating digit grouping",
            "Digit grouping updated",
            VisibilityLevel::Basic,
            {},
            "Result lines"
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
            VisibilityLevel::Basic,
            {},
            "Clear History button"
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
            VisibilityLevel::Advanced,
            {},
            "Session history"
        },
        {
            "behavior/memory/copyHistory",
            CategoryId::Behavior, SubcategoryId::Memory,
            false, ControlType::Action, {}, {},
            "Copy history", "Copy session history",
            "Copies every expression you've run this session to the clipboard",
            "Joins MainWindow::m_history with newlines and writes to QClipboard",
            ApplyMode::Immediate,
            "Copying history to clipboard",
            "History copied",
            VisibilityLevel::Basic,
            {"Copy terminal", "Export history", "Copy calculations"}
            // Action - never enters the pending queue, no `affects` needed
        },

        // --------------------------------------------------------------------
        // BEHAVIOR / ANIMATIONS
        // --------------------------------------------------------------------
        {
            "behavior/animations/showApplyAnimation",
            CategoryId::Behavior, SubcategoryId::Animations,
            "Once", ControlType::Dropdown,
            {"Once", "Always", "Never"}, {},
            "Show apply animation", "Show apply animation",
            "Whether to play the animation when your settings are applied",
            "Controls AnimationMode: Once auto-downgrades to Never after first play",
            ApplyMode::Immediate,
            "Updating animation preference",
            "Animation preference updated",
            VisibilityLevel::Basic,
            {"Apply animation", "Settings animation", "Transition"},
            "Settings page",
            ""
        },

        // --------------------------------------------------------------------
        // BEHAVIOR / DEVELOPER
        // --------------------------------------------------------------------
        {
        "behavior/developer/widgetInspector",
        CategoryId::Behavior, SubcategoryId::Developer,
        false, ControlType::Toggle, {}, {},
        "Widget inspector", "Enable widget inspector",
        "Lets you click any terminal label to view and edit its properties",
        "Enables hover glow + click-to-edit on every MasterLabel-derived widget",
        ApplyMode::Immediate,
        "Toggling widget inspector",
        "Widget inspector updated",
        VisibilityLevel::Developer,
        {"Inspector", "Debug", "Widget editor"},
        "Terminal labels"
        },

        // --------------------------------------------------------------------
        // SYSTEM / DATA
        // --------------------------------------------------------------------
        {
            "system/data/exportSettings",
            CategoryId::System, SubcategoryId::Data,
            false, ControlType::Action, {}, {},
            "Export settings to file", "Export settings",
            "Save all your settings to a file you can share or back up",
            "Serialises QSettings to JSON via QJsonDocument",
            ApplyMode::Immediate,
            "Exporting settings",
            "Settings exported",
            VisibilityLevel::Basic
            // Action - no `affects` needed
        },
        {
            "system/data/importSettings",
            CategoryId::System, SubcategoryId::Data,
            false, ControlType::Action, {}, {},
            "Import settings from file", "Import settings",
            "Load settings from a file exported by MathX",
            "Deserialises JSON into QSettings, validates keys against SettingsDef",
            ApplyMode::Immediate,
            "Importing settings",
            "Settings imported",
            VisibilityLevel::Basic
            // Action - no `affects` needed
        },
        {
            "system/data/resetAll",
            CategoryId::System, SubcategoryId::Data,
            false, ControlType::Action, {}, {},
            "Reset everything to defaults", "Reset all settings",
            "Wipes all your settings and starts fresh",
            "Calls Settings::instance().resetAll() - irreversible without export",
            ApplyMode::Immediate,
            "Resetting all settings to defaults",
            "Settings reset",
            VisibilityLevel::Basic
            // Action - no `affects` needed
        },
    };
    return list;
}

// -- Lookup helpers ------------------------------------------------------------

const SettingDef* findSetting(const QString& key) {
    static const QHash<QString, const SettingDef*> index = []() {
        QHash<QString, const SettingDef*> h;
        for (const SettingDef& def : allSettings())
            h.insert(def.key, &def);
        return h;
        }();
    return index.value(key, nullptr);
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