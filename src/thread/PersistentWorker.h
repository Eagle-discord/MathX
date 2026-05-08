#pragma once
#include <QObject>
#include <QString>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

struct CalcJob {
    int id;
    QString expression;
};

class PersistentWorker : public QObject {
    Q_OBJECT
public:
    explicit PersistentWorker(QObject* parent = nullptr);
    ~PersistentWorker();
    void stop();  
public slots:
    void submitJob(int id, const QString& expr);
    void cancelAll();

signals:
    void resultReady(int id, const QString& result, const QString& type, const QString& formula = QString());
    void finished(); // queue empty
    void progress(int jobId, int progress);

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