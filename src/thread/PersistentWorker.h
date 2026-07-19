#pragma once
#include <QObject>
#include <QString>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>
#include "../constants/ResultTypes.h"

struct CalcJob {
    int id;
    QString expression;
};

class PersistentWorker : public QObject {
    Q_OBJECT
public:


    static std::atomic<bool> s_cancel;
    static bool isCancelled() { return s_cancel.load(); }
    static std::atomic<bool>* cancelFlag() { return &s_cancel; }
    explicit PersistentWorker(QObject* parent = nullptr);
    ~PersistentWorker();
    void stop();
public slots:
    void submitJob(int id, const QString& expr);
    void cancelAll();

signals:

    void resultReady(int id, const QString& result, ResultType type, const QString& formula = QString(), double elapsedMs = 0.0);
    void finished(); // queue empty
    void progress(int jobId, int percent, const QString& label);
public slots:
    void process(); // runs in worker thread

private:
    // sets flag to exit the process loop

    QQueue<CalcJob> m_queue;
    QMutex m_mutex;
    QWaitCondition m_cond;
    std::atomic<bool> m_cancel{ false };
    std::atomic<bool> m_stop{ false };
};