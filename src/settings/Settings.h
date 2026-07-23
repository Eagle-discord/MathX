#pragma once
#include <QObject>
#include <QString>
#include <QVariant>
#include <QSettings>
#include <QList>
#include <QTimer>
#include "RegistryWatcher.h"

// Forward declare - SettingsDef.h defines these fully
struct SettingDef;

// -- PendingChange -------------------------------------------------------------
// Represents a single staged or deferred setting change waiting to be applied.
// Held in the pending queue until applyPending() is called.
struct PendingChange {
    QString key;
    QVariant oldValue;
    QVariant newValue;
    QString label;
    QString applyingLabel;
    QString appliedLabel;

    bool operator==(const PendingChange& other) const {
        return key == other.key;
    }
};

// -- ApplyMode -----------------------------------------------------------------
// Immediate - applies the instant the control changes, never enters the queue
// Staged    - queued, applies when RunState transitions to Idle
// Deferred  - queued, applies when current operation completes
enum class ApplyMode { Immediate, Staged, Deferred };

// -- AnimationMode -------------------------------------------------------------
// Once    - plays the full theatrical apply animation the first time only,
//           then auto-downgrades to Never and shows the info message.
// Always  - full animation every time the user navigates away with pending changes.
// Never   - silent background apply, no animation, no delay.
enum class AnimationMode { Once, Always, Never };

// -- HintState -----------------------------------------------------------------
// Tracks the lifecycle of the visibility level hint
// Active   - first open, hint visible, control glowing
// Passive  - hint on demand via ? icon
// Dormant  - bare label only, no visual noise
enum class HintState { Active, Passive, Dormant };

// -- VisibilityLevel -----------------------------------------------------------
enum class VisibilityLevel { Basic, Advanced, Developer };

// -- Settings ------------------------------------------------------------------
// Singleton that owns all user preferences for MathX.
// Wraps QSettings (Windows registry: HKCU\Software\MathX) so every value
// persists across sessions automatically.
//
// Usage:
//   Settings::instance().get("appearance/typography/fontSize", 10)
//   Settings::instance().set("appearance/typography/fontSize", 12)
//
//   // Typed convenience accessors
//   Settings::instance().fontSize()
//   Settings::instance().setFontSize(12)
//
// Adding a new setting:
//   1. Add its SettingDef entry in SettingsDef.cpp - nothing else needed
//   2. Optionally add a typed getter/setter here for clean consumer code
//
// Apply pipeline:
//   Immediate settings apply the moment set() is called.
//   Staged/Deferred settings enter the pending queue.
//   Call applyPending() to flush the queue (on navigation or RunState::Idle).

class Settings : public QObject {
    Q_OBJECT

public:
    static Settings& instance() {
        static Settings s;
        return s;
    }
    ~Settings();

    // -- Generic access --------------------------------------------------------
    // Reads a value from the store, falling back to the default defined in
    // SettingsDef if not yet set by the user.
    QVariant get(const QString& key) const;

    // Writes a value. If the setting's ApplyMode is Immediate, applies at once
    // and emits the relevant signal. Otherwise stages it in the pending queue.
    void set(const QString& key, const QVariant& value);

    // -- Pending queue ---------------------------------------------------------
    // Returns a snapshot of all currently staged changes for the UI to display.
    const QList<PendingChange>& pendingChanges() const { return m_pending; }
    bool hasPendingChanges() const { return !m_pending.isEmpty(); }

    // Flushes all Staged changes if idle, and all Deferred changes regardless.
    // Called by MainWindow on RunState::Idle transition and on navigation.
    // isIdle - pass true when RunState is Idle, false otherwise.
    void applyPending(bool isIdle);

    // Starts the 800ms debounce timer used in Background animation mode.
    // Resets on every setting interaction. Fires applyPending(true) on timeout.
    void startDebounce();

    // -- UI state --------------------------------------------------------------
    VisibilityLevel visibilityLevel() const { return m_visibilityLevel; }
    void setVisibilityLevel(VisibilityLevel level);

    HintState hintState() const { return m_hintState; }
    void advanceHintState(HintState to);

    AnimationMode animationMode() const { return m_animationMode; }
    void setAnimationMode(AnimationMode mode);

    // Returns true if the apply animation should play for this navigation.
    // Handles the Once auto-downgrade side effect internally:
    //   Once  → returns true first time, then sets mode to Never + hasPlayedFirst = true
    //   Always → always true
    //   Never  → always false
    bool shouldPlayApplyAnimation();

    // True if the "changes apply on leave" hint has been shown to the user.
    // Set to true the first time the pending queue goes non-empty.
    bool hasSeenPendingHint() const { return m_hasSeenPendingHint; }
    void markPendingHintSeen();

    // True if the post-animation "this is now off" info message has been shown.
    bool hasSeenPostAnimationHint() const { return m_hasSeenPostAnimationHint; }
    void markPostAnimationHintSeen();

    // -- Utility ---------------------------------------------------------------
    // Wipes all persisted values and resets UI state to defaults.
    void resetAll();

    // -- Typed convenience accessors -------------------------------------------
    // These exist so consumers can write Settings::instance().fontSize()
    // rather than Settings::instance().get("appearance/typography/fontSize").
    // They always read the active (applied) value, not the pending one.
    //int     fontSize()          const;
    QString fontFamily()        const;
    QString accentColor()       const;
    QString theme()             const;
    bool    splitThreads()      const;
    QString progressStyle()     const;
    bool    showProgressHeavy() const;
    int     truncationLimit()   const;
    int     bigNumThreshold()   const;
    int     streamChunkSize()   const;
    QString angleUnit()         const;
    int     historySize()       const;
    bool    groupDigits()       const;
    QString numberNames()       const;
    bool    confirmClear()      const;
    bool    showPropertyLabels() const;
    QString defaultShapeColor() const;
    int  decimalPlaces() const;
    int fontSizeUI()         const;
    int fontSizeResults()    const;
    int fontSizeSeparators() const;
    int fontSizeInput()      const;
    int fontSizeSidebar()    const;
    int  fontSizeSplash() const;


  //  void setFontSize(int v);
    void setFontFamily(const QString& v);
    void setAccentColor(const QString& v);
    void setTheme(const QString& v);
    void setSplitThreads(bool v);
    void setProgressStyle(const QString& v);
    void setShowProgressHeavy(bool v);
    void setTruncationLimit(int v);
    void setBigNumThreshold(int v);
    void setStreamChunkSize(int v);
    void setAngleUnit(const QString& v);
    void setHistorySize(int v);
    void setConfirmClear(bool v);
    void setDefaultShapeColor(const QString& v);
    void setDecimalPlaces(int v);
    void setFontSizeUI(int v);
    void setFontSizeResults(int v);
    void setFontSizeSeparators(int v);
    void setFontSizeInput(int v);
    void setFontSizeSidebar(int v);
    void setFontSizeSplash(int v);
signals:
    // -- Immediate signals - fire the moment the setting changes ---------------
    void fontSizeChanged(int newSize);
    void fontFamilyChanged(const QString& newFamily);
    void accentColorChanged(const QString& newColor);
    void themeChanged(const QString& newTheme);
    void progressStyleChanged(const QString& newStyle);
    void showProgressHeavyChanged(bool enabled);
    void truncationLimitChanged(int newLimit);
    void angleUnitChanged(const QString& newUnit);
    void historySizeChanged(int newSize);
    void groupDigitsChanged(bool enabled);
    void numberNamesChanged(const QString& mode);
    void confirmClearChanged(bool enabled);
    void showPropertyLabelsChanged(bool enabled);
    void defaultShapeColorChanged(const QString& newColor);
    void decimalPlacesChanged(int places);
    void fontSizeUIChanged(int pt);
    void fontSizeResultsChanged(int pt);
    void fontSizeSeparatorsChanged(int pt);
    void fontSizeInputChanged(int pt);
    void fontSizeSidebarChanged(int pt);

    void fontSizeSplashChanged(int pt);


    // -- Staged signals - fire when applyPending() commits them ----------------
    void splitThreadsChanged(bool enabled);
    void bigNumThresholdChanged(int newThreshold);
    void streamChunkSizeChanged(int newSize);

    // -- UI state signals ------------------------------------------------------
    void visibilityLevelChanged(VisibilityLevel newLevel);
    void hintStateChanged(HintState newState);
    void animationModeChanged(AnimationMode newMode);

    // -- Queue signals ---------------------------------------------------------
    void pendingChanged();          // queue was modified - UI should refresh
    void pendingApplied(const QList<PendingChange>& applied); // apply complete
    void pendingHintNeeded();       // first-ever pending change - show the hint once

    // -- Reset -----------------------------------------------------------------
    void settingsReset();

    // -- External registry change -----------------------------------------------
    // Emitted when HKCU\Software\MathX is modified externally (regedit, script).
    // changedKeys lists every key whose value differs from our in-memory copy.
    // All relevant typed signals are also emitted, so most consumers don't need
    // to connect to this directly - it's mainly for debug/diagnostics.
    void externalChangeApplied(QStringList changedKeys);

private:
    explicit Settings(QObject* parent = nullptr);
    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    // Applies a single change immediately, emits its typed signal.
    void applyChange(const PendingChange& change);

    // Looks up the default value for a key from SettingsDef.
    QVariant defaultFor(const QString& key) const;

    // Looks up the ApplyMode for a key from SettingsDef.
    ApplyMode applyModeFor(const QString& key) const;

    // Builds the enriched label for the pending queue footer:
    //   "{labelBasic} - {affects}"  when affects is set
    //   "{labelBasic}"              otherwise
    QString buildPreviewLabel(const SettingDef* def, const QVariant& newValue) const;

    // Called when the registry watcher detects an external change.
    // Diffs m_snapshot against the current registry, emits typed signals
    // for every key that changed, and updates m_snapshot.
    void onRegistryChanged();


    // In-memory snapshot of the last-known registry state for all known
    // setting keys. Used to diff against after an external change fires,
    // so we know *which* keys actually changed rather than re-reading all.
    QHash<QString, QVariant> m_snapshot;

    QSettings           m_store;
    QList<PendingChange> m_pending;
    QTimer* m_debounce = nullptr;
    VisibilityLevel     m_visibilityLevel = VisibilityLevel::Basic;
    HintState           m_hintState = HintState::Active;
    AnimationMode       m_animationMode = AnimationMode::Once;
    bool                m_hasSeenPendingHint = false;
    bool                m_hasSeenPostAnimationHint = false;

    // -- Registry watcher ------------------------------------------------------
    // Watches HKCU\Software\MathX for external changes so tools like regedit
    // or scripts can modify settings and have the app react live.
    RegistryWatcher* m_watcher = nullptr;
};