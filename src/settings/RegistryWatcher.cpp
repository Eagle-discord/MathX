#include "RegistryWatcher.h"
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <QMetaObject>
#endif

RegistryWatcher::RegistryWatcher(QObject* parent) : QObject(parent) {
    // Move this object to the background thread so watch() runs there
    qDebug("RegistryWatcher CONSTRUCTOR");
    moveToThread(&m_thread);
    qDebug("RegistryWatcher moved to thread");
    qDebug("End of registryWatcher Constructor");
}

RegistryWatcher::~RegistryWatcher() {
    stop();
}

void RegistryWatcher::start() {
    m_stop = false;
    m_thread.start();
    // Post watch() to the object's thread (the background thread)
    QMetaObject::invokeMethod(this, [this]() {
        watch();
        }, Qt::QueuedConnection);
}

void RegistryWatcher::stop() {
    m_stop = true;
    m_thread.quit();
    m_thread.wait(2000);
}

void RegistryWatcher::watch() {

    qDebug("RegistryWatcher::watch ");
#ifdef Q_OS_WIN
    // Open the key we want to watch.
    // QSettings(NativeFormat, UserScope, "MathX") maps to:
    //   HKCU\Software\MathX
    qDebug() << "RegistryWatcher::watch running on thread:" << QThread::currentThread();
    HKEY hKey = nullptr;
    qDebug("Reading key");
    LONG result = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\MathX",
        0,
        KEY_NOTIFY | KEY_READ,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        // Key doesn't exist yet (first run before any setting is written).
        // Exit the watcher — it will be restarted if needed.
        return;
    }

    // Create a manual-reset Win32 event. RegNotifyChangeKeyValue signals this
    // event when the registry key changes, which unblocks WaitForSingleObject.
    HANDLE hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!hEvent) {
        RegCloseKey(hKey);
        return;
    }

    while (!m_stop) {
        // Register for notification. REG_NOTIFY_CHANGE_LAST_SET fires when
        // any value under this key is created, deleted, or modified.
        // bWatchSubtree = TRUE covers nested keys too (e.g. Software\MathX\appearance\...).
        result = RegNotifyChangeKeyValue(
            hKey,
            TRUE,                             // watch subtree
            REG_NOTIFY_CHANGE_LAST_SET |      // value changes
            REG_NOTIFY_CHANGE_NAME,           // key renames/deletes
            hEvent,
            TRUE                              // asynchronous — don't block here
        );

        if (result != ERROR_SUCCESS) break;

        // Block until the event fires or we're asked to stop (checked every 250ms)
        while (!m_stop) {
            DWORD wait = WaitForSingleObject(hEvent, 250);
            if (wait == WAIT_OBJECT_0) {
                // Registry changed — reset the event and emit signal
                ResetEvent(hEvent);
                emit registryChanged();
                break;
            }
            // WAIT_TIMEOUT → check m_stop and re-wait
        }
    }

    CloseHandle(hEvent);
    RegCloseKey(hKey);

#else
    // No-op on non-Windows platforms — the watcher thread exits immediately
    Q_UNUSED(this)
#endif
}