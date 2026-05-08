#include "GenericWorker.h"
#include "../math/MathEngine.h"
#include <QThread>

void GenericWorker::compute(const QString& expr) {
    if (m_cancelled) {
        emit finished("Cancelled", "err");
        return;
    }

    CalcResult res;
    try {
        res = MathEngine::evaluate(expr);
    }
    catch (const std::exception& e) {
        emit finished(QString("Error: ") + e.what(), "err");
        return;
    }

    if (m_cancelled) {
        emit finished("Cancelled", "err");
        return;
    }
    emit finished(res.result, res.type);
}

void GenericWorker::cancel() {
    m_cancelled = true;
    // Request interruption so that long loops in BigNum can exit
    QThread::currentThread()->requestInterruption();
}