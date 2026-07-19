#include "PersistentWorker.h"
#include "../math/MathEngine.h"
#include <QThread>
#include <QRegularExpression>
#include <functional>
#include <QElapsedTimer>



PersistentWorker::PersistentWorker(QObject* parent) : QObject(parent) {}
std::atomic<bool> PersistentWorker::s_cancel{ false };
PersistentWorker::~PersistentWorker() {
    m_stop = true;
    {

        QMutexLocker locker(&m_mutex);
        m_cond.wakeOne();
    }
}

void PersistentWorker::submitJob(int id, const QString& expr) {
    QMutexLocker locker(&m_mutex);
    m_queue.enqueue({ id, expr });
    m_cond.wakeOne();
}

void PersistentWorker::cancelAll() {
    QMutexLocker locker(&m_mutex);
    m_queue.clear();
    s_cancel = true;
    m_cancel = true;
}
void PersistentWorker::stop() {
    m_stop = true;
    {
        QMutexLocker locker(&m_mutex);
        m_cond.wakeOne();   // wake up if waiting
    }
}

void PersistentWorker::process() {
    while (!m_stop) {
        CalcJob job;
        {
            QMutexLocker locker(&m_mutex);
            while (!m_stop && m_queue.isEmpty())
                m_cond.wait(&m_mutex);
            if (m_stop || m_queue.isEmpty()) continue;
            job = m_queue.dequeue();
            s_cancel = false;
            m_cancel = false;
        }

        // Check cancellation before starting
        if (QThread::currentThread()->isInterruptionRequested()) {
            emit resultReady(job.id, "Cancelled", ResultType::err);
            continue;
        }
        QElapsedTimer calcTime;
        calcTime.start();

        static QRegularExpression factRe(R"(^\s*(?:fact\s*\(\s*(\d+)\s*\)|(\d+)\s*!)\s*$)");
        auto m = factRe.match(job.expression);
        if (m.hasMatch()) {
            QString nStr = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
            BigInt n(nStr.toStdString());
            if (n < 0) {
                emit resultReady(job.id, "Negative factorial", ResultType::err);
                continue;
            }

            try {
                // Two phases, because they're genuinely two different waits: the
                // multiply loop, then the base-2 -> base-10 conversion of the
                // result. For 100000! the second is not a rounding error -- it's
                // converting a 456574-digit number, and it used to present as a
                // freeze at 100%.
                auto callback = [this, jobId = job.id](int percent) {
                    emit progress(jobId, percent, "Calculating");
                    };
                auto phaseCb = [this, jobId = job.id](const QString& phase) {
                    emit progress(jobId, 100, phase);
                    };
                QString result = BigNum::bigFactorial(n, callback, phaseCb);
                double ms = calcTime.nsecsElapsed() / 1'000'000.0;
                emit resultReady(job.id, result, ResultType::big, "", ms);
            }
            catch (const std::exception& e) {
                double ms = calcTime.nsecsElapsed() / 1'000'000.0;
                emit resultReady(job.id, QString("Error: ") + e.what(), ResultType::err, "", ms);
            }
            continue;
        }

        // Standalone exponentiation a^b
        static QRegularExpression powRe(R"(^\s*(\d+)\s*\^\s*(\d+)\s*$)");
        auto powMatch = powRe.match(job.expression);
        if (powMatch.hasMatch()) {
            BigInt base(powMatch.captured(1).toStdString());
            BigInt exp(powMatch.captured(2).toStdString());
            if (exp < 0) {
                emit resultReady(job.id, "Negative exponent not supported", ResultType::err);
                continue;
            }
            try {
                auto progressCb = [this, jobId = job.id](int percent) {
                    emit progress(jobId, percent, "Calculating");
                    };
                // s_cancel, not m_cancel. cancelAll() sets both, so the old code
                // did work -- but every other cancellable path (bigFactorial,
                // factorialBig) polls the static one, and two flags meaning the
                // same thing is how you end up with a Stop button that works for
                // three of four operations. One flag.
                QString result = BigNum::bigPow(base, exp, progressCb, &s_cancel);
                double ms = calcTime.nsecsElapsed() / 1'000'000.0;
                emit resultReady(job.id, result, ResultType::big, "", ms);
            }
            catch (const std::exception& e) {
                double ms = calcTime.nsecsElapsed() / 1'000'000.0;
                emit resultReady(job.id, QString("Error: ") + e.what(), ResultType::err, "", ms);
            }
            continue;
        }

        CalcResult res;
        try {
            res = MathEngine::evaluate(job.expression);
        }
        catch (const std::exception& e) {
            double ms = calcTime.nsecsElapsed() / 1'000'000.0;
            emit resultReady(job.id, QString("Error: ") + e.what(), ResultType::err, "", ms);
            continue;
        }

        if (QThread::currentThread()->isInterruptionRequested()) {
            double ms = calcTime.nsecsElapsed() / 1'000'000.0;
            emit resultReady(job.id, "Cancelled", ResultType::err, "", ms);
        }
        else {
            double ms = calcTime.nsecsElapsed() / 1'000'000.0;
            emit resultReady(job.id, res.result, res.type, res.formula, ms);
        }
    }
}