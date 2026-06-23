#pragma once
#include <QObject>
#include <QThread>
#include <atomic>

// -- RegistryWatcher -----------------------------------------------------------
// Watches HKCU\Software\MathX for external changes (regedit, scripts, other
// processes) and emits registryChanged() when any value is modified.
//
// Runs on its own QThread using RegNotifyChangeKeyValue — a blocking Win32 API
// that sleeps until the OS signals a change. This avoids polling entirely and
// has effectively zero CPU cost at rest.
//
// Only compiled on Windows. On other platforms the class is a no-op stub so
// the rest of the codebase can unconditionally include this header.

class RegistryWatcher : public QObject {
    Q_OBJECT

public:
    explicit RegistryWatcher(QObject* parent = nullptr);
    ~RegistryWatcher();

    // Starts the watcher on a background thread. Safe to call from any thread.
    void start();

    // Signals the background thread to exit and waits for it to finish.
    void stop();

signals:
    // Emitted on the watcher's thread — connect with Qt::QueuedConnection
    // (or connect to a slot on the main thread, Qt handles the queue automatically).
    // changedKey is a best-effort hint at what changed — may be empty if the
    // OS only reported that *something* under the key changed.
    void registryChanged();

private slots:
    void watch(); // runs on m_thread, blocks in RegNotifyChangeKeyValue loop

private:
    QThread        m_thread;
    std::atomic<bool> m_stop{ false };
};