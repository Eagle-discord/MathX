#include "Settings.h"
#include "SettingsDef.h"
#include "RegistryWatcher.h"

// ── Destructor ────────────────────────────────────────────────────────────────

Settings::~Settings() {
    if (m_watcher) m_watcher->stop();
}

// ── Constructor ───────────────────────────────────────────────────────────────

Settings::Settings(QObject* parent)
    : QObject(parent)
    , m_store(QSettings::NativeFormat, QSettings::UserScope, "MathX")
{
    // Restore persisted UI state
    m_visibilityLevel = static_cast<VisibilityLevel>(
        m_store.value("__ui/visibilityLevel", 0).toInt());
    m_hintState = static_cast<HintState>(
        m_store.value("__ui/hintState", 0).toInt());
    m_animationMode = static_cast<AnimationMode>(
        m_store.value("__ui/animationMode", 0).toInt()); // 0 = Once
    m_hasSeenPendingHint = m_store.value("__ui/hasSeenPendingHint", false).toBool();
    m_hasSeenPostAnimationHint = m_store.value("__ui/hasSeenPostAnimationHint", false).toBool();

    // Debounce timer — used when animationMode == Never to apply silently
    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(800);
    connect(m_debounce, &QTimer::timeout, this, [this]() {
        applyPending(true);
        });

    // Build initial snapshot — baseline for diffing external changes
    for (const SettingDef& def : allSettings())
        m_snapshot[def.key] = m_store.value(def.key, def.defaultValue);

    // Start registry watcher — detects external writes to HKCU\Software\MathX
    m_watcher = new RegistryWatcher;
    connect(m_watcher, &RegistryWatcher::registryChanged,
        this, &Settings::onRegistryChanged, Qt::QueuedConnection);
    // Start the watcher after the event loop is fully running
    QTimer::singleShot(0, this, [this]() {
        if (m_watcher) m_watcher->start();
        });
}

// ── Generic access ────────────────────────────────────────────────────────────

QVariant Settings::get(const QString& key) const {
    return m_store.value(key, defaultFor(key));
}

void Settings::set(const QString& key, const QVariant& value) {
    const ApplyMode mode = applyModeFor(key);

    if (mode == ApplyMode::Immediate) {
        // Write directly — no queue, no wait
        m_store.setValue(key, value);
        m_store.sync();
        m_snapshot[key] = value; // keep snapshot in sync so watcher doesn't re-fire

        const SettingDef* def = findSetting(key);
        PendingChange change;
        change.key = key;
        change.oldValue = get(key);
        change.newValue = value;
        change.label = buildPreviewLabel(def, value);
        change.applyingLabel = def ? def->applyingLabel : QString();
        change.appliedLabel = def ? def->appliedLabel : QString();
        applyChange(change);
        return;
    }

    // Staged or Deferred — check if key already has a pending entry
    for (PendingChange& existing : m_pending) {
        if (existing.key == key) {
            // User reverted to the original value — remove the entry entirely
            if (existing.oldValue == value) {
                m_pending.removeOne(existing);
                emit pendingChanged();
                return;
            }
            // Update the pending value in place, keep original oldValue
            existing.newValue = value;
            emit pendingChanged();
            if (m_animationMode == AnimationMode::Never)
                startDebounce();
            return;
        }
    }

    // Brand new pending entry
    const SettingDef* def = findSetting(key);
    PendingChange change;
    change.key = key;
    change.oldValue = get(key);
    change.newValue = value;
    change.label = buildPreviewLabel(def, value);
    change.applyingLabel = def ? def->applyingLabel : QString();
    change.appliedLabel = def ? def->appliedLabel : QString();
    m_pending.append(change);
    emit pendingChanged();

    // First pending change ever — show the "applies on leave" hint
    if (!m_hasSeenPendingHint)
        emit pendingHintNeeded();

    if (m_animationMode == AnimationMode::Never)
        startDebounce();
}

// ── Pending queue ─────────────────────────────────────────────────────────────

void Settings::applyPending(bool isIdle) {
    if (m_pending.isEmpty()) return;

    QList<PendingChange> applied;
    QMutableListIterator<PendingChange> it(m_pending);

    while (it.hasNext()) {
        PendingChange& change = it.next();
        const ApplyMode mode = applyModeFor(change.key);

        // Staged only commits when RunState is Idle
        if (mode == ApplyMode::Staged && !isIdle) continue;

        m_store.setValue(change.key, change.newValue);
        m_store.sync();
        m_snapshot[change.key] = change.newValue; // keep snapshot in sync
        applyChange(change);

        applied.append(change);
        it.remove();
    }

    if (!applied.isEmpty()) {
        emit pendingChanged();
        emit pendingApplied(applied);
    }
}

void Settings::startDebounce() {
    if (m_debounce) m_debounce->start(); // restarts timer if already running
}

// ── UI state ──────────────────────────────────────────────────────────────────

void Settings::setVisibilityLevel(VisibilityLevel level) {
    if (m_visibilityLevel == level) return;
    m_visibilityLevel = level;
    m_store.setValue("__ui/visibilityLevel", static_cast<int>(level));
    m_store.sync();

    // First change to visibility level advances hint from Passive → Dormant
    if (m_hintState == HintState::Passive)
        advanceHintState(HintState::Dormant);

    emit visibilityLevelChanged(level);
}

void Settings::advanceHintState(HintState to) {
    if (m_hintState == to) return;
    // One-way only: Active → Passive → Dormant, never backwards
    if (static_cast<int>(to) <= static_cast<int>(m_hintState)) return;
    m_hintState = to;
    m_store.setValue("__ui/hintState", static_cast<int>(to));
    m_store.sync();
    emit hintStateChanged(to);
}

void Settings::setAnimationMode(AnimationMode mode) {
    if (m_animationMode == mode) return;
    m_animationMode = mode;
    m_store.setValue("__ui/animationMode", static_cast<int>(mode));
    m_store.sync();
    emit animationModeChanged(mode);
}

bool Settings::shouldPlayApplyAnimation() {
    switch (m_animationMode) {
    case AnimationMode::Always:
        return true;
    case AnimationMode::Never:
        return false;
    case AnimationMode::Once:
        // First time — auto-downgrade to Never so it never plays again
        // unless the user explicitly sets Always.
        setAnimationMode(AnimationMode::Never);
        return true;
    }
    return false;
}

void Settings::markPendingHintSeen() {
    if (m_hasSeenPendingHint) return;
    m_hasSeenPendingHint = true;
    m_store.setValue("__ui/hasSeenPendingHint", true);
    m_store.sync();
}

void Settings::markPostAnimationHintSeen() {
    if (m_hasSeenPostAnimationHint) return;
    m_hasSeenPostAnimationHint = true;
    m_store.setValue("__ui/hasSeenPostAnimationHint", true);
    m_store.sync();
}

// ── Utility ───────────────────────────────────────────────────────────────────

void Settings::resetAll() {
    m_pending.clear();
    m_store.clear();
    m_store.sync();

    // Rebuild snapshot from defaults so the watcher doesn't treat every
    // key as "externally changed" after the registry is wiped and refilled
    m_snapshot.clear();
    for (const SettingDef& def : allSettings())
        m_snapshot[def.key] = def.defaultValue;

    // Reset UI state to factory defaults
    m_visibilityLevel = VisibilityLevel::Basic;
    m_hintState = HintState::Active;
    m_animationMode = AnimationMode::Once;
    m_hasSeenPendingHint = false;
    m_hasSeenPostAnimationHint = false;

    emit settingsReset();
    emit pendingChanged();
    emit visibilityLevelChanged(m_visibilityLevel);
    emit hintStateChanged(m_hintState);
    emit animationModeChanged(m_animationMode);
}

// ── Private helpers ───────────────────────────────────────────────────────────

QVariant Settings::defaultFor(const QString& key) const {
    const SettingDef* def = findSetting(key);
    return def ? def->defaultValue : QVariant();
}

// Builds the enriched preview label for the pending queue:
//   With affects:  "Font size - Result labels"
//   Without:       "Font size"
// The footer then appends ": {old}{unit} → {new}{unit}" around it.
QString Settings::buildPreviewLabel(const SettingDef* def, const QVariant& /*newValue*/) const {
    if (!def) return QString();
    if (!def->affects.isEmpty())
        return QString("%1 - %2").arg(def->labelBasic, def->affects);
    return def->labelBasic;
}

// ── External registry change handler ─────────────────────────────────────────
// Called on the main thread (queued connection from the watcher thread).
// Re-reads every known setting key from the registry, diffs against m_snapshot,
// fires typed signals for anything that changed, then updates the snapshot.
void Settings::onRegistryChanged() {
    // Force QSettings to re-read from disk/registry
    m_store.sync();

    QStringList changedKeys;

    for (const SettingDef& def : allSettings()) {
        const QVariant fresh = m_store.value(def.key, def.defaultValue);
        const QVariant cached = m_snapshot.value(def.key, def.defaultValue);

        if (fresh == cached) continue;

        // Something changed externally — treat it as an immediate apply.
        // We skip the pending queue entirely since this came from outside
        // the app; there's nothing to "stage" — it's already in the registry.
        m_snapshot[def.key] = fresh;
        changedKeys.append(def.key);

        // Re-use the same applyChange() dispatch so typed signals fire
        // identically whether the change came from the UI or from regedit.
        PendingChange synthetic;
        synthetic.key = def.key;
        synthetic.oldValue = cached;
        synthetic.newValue = fresh;
        synthetic.label = buildPreviewLabel(&def, fresh);
        applyChange(synthetic);
    }

    if (!changedKeys.isEmpty())
        emit externalChangeApplied(changedKeys);
}


ApplyMode Settings::applyModeFor(const QString& key) const {
    const SettingDef* def = findSetting(key);
    return def ? def->applyMode : ApplyMode::Immediate;
}

void Settings::applyChange(const PendingChange& change) {
    // The one place that maps setting keys to typed signals.
    // Consumers connect to typed signals — never to a generic settingChanged().
    const QString& k = change.key;
    const QVariant& v = change.newValue;

    if (k == "appearance/typography/fontSize")       emit fontSizeChanged(v.toInt());
    else if (k == "appearance/typography/fontFamily")     emit fontFamilyChanged(v.toString());
    else if (k == "appearance/colors/accentColor")        emit accentColorChanged(v.toString());
    else if (k == "appearance/theme/preset")              emit themeChanged(v.toString());
    else if (k == "display/progress/style")               emit progressStyleChanged(v.toString());
    else if (k == "display/progress/showForHeavyOnly")    emit showProgressHeavyChanged(v.toBool());
    else if (k == "display/results/truncationLimit")      emit truncationLimitChanged(v.toInt());
    else if (k == "display/geometry/autoRotate")          emit autoRotateChanged(v.toBool());
    else if (k == "display/geometry/defaultShapeColor")   emit defaultShapeColorChanged(v.toString());
    else if (k == "behavior/threading/splitThreads")      emit splitThreadsChanged(v.toBool());
    else if (k == "behavior/computation/angleUnit")       emit angleUnitChanged(v.toString());
    else if (k == "behavior/computation/bigNumThreshold") emit bigNumThresholdChanged(v.toInt());
    else if (k == "behavior/computation/streamChunkSize") emit streamChunkSizeChanged(v.toInt());
    else if (k == "behavior/memory/historySize")          emit historySizeChanged(v.toInt());
    else if (k == "behavior/memory/confirmClear")         emit confirmClearChanged(v.toBool());
}

// ── Typed getters ─────────────────────────────────────────────────────────────

int     Settings::fontSize()          const { return get("appearance/typography/fontSize").toInt(); }
QString Settings::fontFamily()        const { return get("appearance/typography/fontFamily").toString(); }
QString Settings::accentColor()       const { return get("appearance/colors/accentColor").toString(); }
QString Settings::theme()             const { return get("appearance/theme/preset").toString(); }
bool    Settings::splitThreads()      const { return get("behavior/threading/splitThreads").toBool(); }
QString Settings::progressStyle()     const { return get("display/progress/style").toString(); }
bool    Settings::showProgressHeavy() const { return get("display/progress/showForHeavyOnly").toBool(); }
int     Settings::truncationLimit()   const { return get("display/results/truncationLimit").toInt(); }
int     Settings::bigNumThreshold()   const { return get("behavior/computation/bigNumThreshold").toInt(); }
int     Settings::streamChunkSize()   const { return get("behavior/computation/streamChunkSize").toInt(); }
QString Settings::angleUnit()         const { return get("behavior/computation/angleUnit").toString(); }
int     Settings::historySize()       const { return get("behavior/memory/historySize").toInt(); }
bool    Settings::confirmClear()      const { return get("behavior/memory/confirmClear").toBool(); }
bool    Settings::autoRotate()        const { return get("display/geometry/autoRotate").toBool(); }
QString Settings::defaultShapeColor() const { return get("display/geometry/defaultShapeColor").toString(); }

// ── Typed setters ─────────────────────────────────────────────────────────────

void Settings::setFontSize(int v) { set("appearance/typography/fontSize", v); }
void Settings::setFontFamily(const QString& v) { set("appearance/typography/fontFamily", v); }
void Settings::setAccentColor(const QString& v) { set("appearance/colors/accentColor", v); }
void Settings::setTheme(const QString& v) { set("appearance/theme/preset", v); }
void Settings::setSplitThreads(bool v) { set("behavior/threading/splitThreads", v); }
void Settings::setProgressStyle(const QString& v) { set("display/progress/style", v); }
void Settings::setShowProgressHeavy(bool v) { set("display/progress/showForHeavyOnly", v); }
void Settings::setTruncationLimit(int v) { set("display/results/truncationLimit", v); }
void Settings::setBigNumThreshold(int v) { set("behavior/computation/bigNumThreshold", v); }
void Settings::setStreamChunkSize(int v) { set("behavior/computation/streamChunkSize", v); }
void Settings::setAngleUnit(const QString& v) { set("behavior/computation/angleUnit", v); }
void Settings::setHistorySize(int v) { set("behavior/memory/historySize", v); }
void Settings::setConfirmClear(bool v) { set("behavior/memory/confirmClear", v); }
void Settings::setAutoRotate(bool v) { set("display/geometry/autoRotate", v); }
void Settings::setDefaultShapeColor(const QString& v) { set("display/geometry/defaultShapeColor", v); }