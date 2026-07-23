#pragma once
#include <QObject>
#include <QThread>
#include <atomic>

#ifndef Q_OS_WIN
#include <QFileSystemWatcher>
#include <QString>
#endif

// -- RegistryWatcher -----------------------------------------------------------
// Watches the app's persisted settings for external changes (another process, a
// hand edit) and emits registryChanged() when anything is modified.
//
// The NAME is historical -- on Windows the backing store is the registry
// (HKCU\Software\MathX) and this uses RegNotifyChangeKeyValue, a blocking Win32
// API that sleeps until the OS signals a change, on its own QThread for zero
// CPU at rest.
//
// On Linux/macOS QSettings::NativeFormat is not a registry -- it's an INI file
// under ~/.config (Linux) or a plist (macOS). There is nothing to "watch" in a
// registry sense, so the closest faithful recreation is a QFileSystemWatcher on
// that settings file: same outcome (external edits trigger a live reload),
// native mechanism, no thread needed because file watching is already
// event-driven. The interface and the registryChanged() signal are identical
// across platforms, so Settings.cpp needs no #ifdefs.

class RegistryWatcher : public QObject {
    Q_OBJECT

public:
    explicit RegistryWatcher(QObject* parent = nullptr);
    ~RegistryWatcher();

    // Starts the watcher. Safe to call from any thread.
    void start();

    // Signals the watcher to stop and waits for it to finish.
    void stop();

signals:
    // Emitted when the backing settings store changes externally.
    // Windows: fires on the watcher thread -- connect with Qt::QueuedConnection.
    // Other platforms: fires on the owning thread.
    void registryChanged();

private slots:
    void watch(); // Windows: blocks in RegNotifyChangeKeyValue loop on m_thread

private:
    QThread            m_thread;
    std::atomic<bool>  m_stop{ false };

#ifndef Q_OS_WIN
    // Linux/macOS: event-driven file watch, no thread. Rebound after every
    // change because some editors replace the file (write-new-then-rename)
    // rather than editing in place, which drops the original inode's watch.
    QFileSystemWatcher m_fsWatcher;
    QString            m_settingsPath;
    void rebindFileWatch();
#endif
};