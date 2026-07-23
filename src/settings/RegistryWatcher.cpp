#include "RegistryWatcher.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QMetaObject>
#else
#include <QSettings>
#include <QFileInfo>
#include <QTimer>
#endif

// =============================================================================
//  Windows: blocking RegNotifyChangeKeyValue on a background thread.
//  Linux/macOS: event-driven QFileSystemWatcher on the QSettings INI/plist.
//  The two share only the class shape and the registryChanged() signal.
// =============================================================================

#ifdef Q_OS_WIN

RegistryWatcher::RegistryWatcher(QObject* parent) : QObject(parent) {
    // Move this object to the background thread so watch() runs there.
    moveToThread(&m_thread);
}

RegistryWatcher::~RegistryWatcher() { stop(); }

void RegistryWatcher::start() {
    m_stop = false;
    m_thread.start();
    // Post watch() to the object's own (background) thread.
    QMetaObject::invokeMethod(this, [this]() { watch(); }, Qt::QueuedConnection);
}

void RegistryWatcher::stop() {
    m_stop = true;
    m_thread.quit();
    m_thread.wait(2000);
}

void RegistryWatcher::watch() {
    // QSettings(NativeFormat, UserScope, "MathX") maps to HKCU\Software\MathX.
    HKEY hKey = nullptr;
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER, L"Software\\MathX", 0, KEY_NOTIFY | KEY_READ, &hKey);

    if (result != ERROR_SUCCESS) {
        // Key doesn't exist yet (first run before any setting is written).
        return;
    }

    // Manual-reset event that RegNotifyChangeKeyValue signals on change.
    HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEvent) { RegCloseKey(hKey); return; }

    while (!m_stop) {
        result = RegNotifyChangeKeyValue(
            hKey,
            TRUE,                                            // watch subtree
            REG_NOTIFY_CHANGE_LAST_SET | REG_NOTIFY_CHANGE_NAME,
            hEvent,
            TRUE);                                           // asynchronous
        if (result != ERROR_SUCCESS) break;

        // Block until the event fires or we're asked to stop (polled every 250ms).
        while (!m_stop) {
            DWORD wait = WaitForSingleObject(hEvent, 250);
            if (wait == WAIT_OBJECT_0) {
                ResetEvent(hEvent);
                emit registryChanged();
                break;
            }
        }
    }

    CloseHandle(hEvent);
    RegCloseKey(hKey);
}

#else   // ---- Linux / macOS ------------------------------------------------

RegistryWatcher::RegistryWatcher(QObject* parent) : QObject(parent) {
    // No thread: QFileSystemWatcher is event-driven and delivers on the owning
    // thread's event loop. Resolve the same file QSettings writes to, so we
    // watch exactly what an external edit would touch.
    QSettings probe(QSettings::NativeFormat, QSettings::UserScope, "MathX");
    m_settingsPath = probe.fileName();   // ~/.config/MathX.conf, or the plist

    connect(&m_fsWatcher, &QFileSystemWatcher::fileChanged,
        this, [this](const QString&) {
            emit registryChanged();
            // Editors that save via write-new-then-rename replace the inode, which
            // silently drops the watch. Re-add on a short delay -- immediately after
            // the rename the path may not exist yet for a beat.
            QTimer::singleShot(50, this, [this]() { rebindFileWatch(); });
        });
}

RegistryWatcher::~RegistryWatcher() { stop(); }

void RegistryWatcher::rebindFileWatch() {
    if (m_settingsPath.isEmpty()) return;
    if (!m_fsWatcher.files().contains(m_settingsPath)
        && QFileInfo::exists(m_settingsPath)) {
        m_fsWatcher.addPath(m_settingsPath);
    }
}

void RegistryWatcher::start() {
    m_stop = false;
    // The file won't exist until the first setting is written. addPath fails
    // silently on a missing path, so rebindFileWatch guards with exists().
    rebindFileWatch();
}

void RegistryWatcher::stop() {
    m_stop = true;
    if (!m_fsWatcher.files().isEmpty())
        m_fsWatcher.removePaths(m_fsWatcher.files());
}

// watch() only has meaning in the Windows threaded design; never called here.
void RegistryWatcher::watch() {}

#endif