#include "FactorialWorker.h"
#include <boost/multiprecision/cpp_int.hpp>
#include <QThread>


using boost::multiprecision::cpp_int;


void FactorialWorker::InterruptionRequest() {
    QThread::currentThread()->requestInterruption(); 
}


void FactorialWorker::computeFactorial(cpp_int n) {
    if (n < 0) {
        emit finished("Error: negative factorial");
        return;
    }
    if (n > 100000) {
        emit finished("Warning: Factorial number larger than 100000, calculations may take hours to finish");
        
    }
    cpp_int result = 1;
    
    for (int i = 2; i <= n; ++i) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            emit finished("Factorial operation cancelled by user.");
            
            return;
        }
        
        
        result *= i;
        cpp_int percent = (i * 100) / n;

       QThread::msleep(1);
        
        emit progress(percent);
    }
    emit finished(QString::fromStdString(result.str()));
}


