#pragma once
#include <QObject>
#include <boost/multiprecision/cpp_int.hpp>
using boost::multiprecision::cpp_int;

class FactorialWorker : public QObject {
    Q_OBJECT
public:
    void computeFactorial(cpp_int n);
public slots:
    void InterruptionRequest();
signals:
    void progress(cpp_int percent);
    void finished(const QString& result);

#ifdef __INTELLISENSE__
    // Dummy definitions to silence IntelliSense - never compiled
    void progress(cpp_int) {}
    void finished(const QString&) {}
#endif
};