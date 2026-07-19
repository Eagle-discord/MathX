#pragma once
#include <QObject>
#include <QString>
#include <atomic>
#include "../constants/ResultTypes.h"

class GenericWorker : public QObject {
    Q_OBJECT
public:
    void compute(const QString& expr);
public slots:
    void cancel();
signals:
    void finished(const QString& result, ResultType type);
private:
    std::atomic<bool> m_cancelled{ false };
};