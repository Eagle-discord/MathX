#pragma once
#include <QObject>
#include <QString>
#include <atomic>

class GenericWorker : public QObject {
    Q_OBJECT
public:
    void compute(const QString& expr);
public slots:
    void cancel();
signals:
    void finished(const QString& result, const QString& type);
private:
    std::atomic<bool> m_cancelled{ false };
};