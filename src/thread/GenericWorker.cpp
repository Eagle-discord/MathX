#include "GenericWorker.h"
#include "../math/MathEngine.h"

void GenericWorker::compute(const QString& expr) {
    if (m_cancelled) {
        emit finished("Cancelled", ResultType::err);
        return;
    }

    CalcResult res;
    try {
        res = MathEngine::evaluate(expr);
    }
    catch (const std::exception& e) {
        emit finished(QString("Error: ") + e.what(), ResultType::err);
        return;
    }

    if (m_cancelled) {
        emit finished("Cancelled", ResultType::err);
        return;
    }
    emit finished(res.result, res.type);
}

void GenericWorker::cancel() {
    // m_cancelled is atomic and is the cross-thread cancel signal: cancel()
    // runs on the caller (UI) thread while compute() runs on the worker thread,
    // which checks the flag before and after evaluation. (The previous code
    // called requestInterruption() on QThread::currentThread() here - that is
    // the UI thread, not the worker, so it never reached the running job.)
    m_cancelled = true;
}