#include "PersistentWorker.h"
#include "../math/MathEngine.h"
#include <QThread>
#include <QRegularExpression>
#include <functional>

PersistentWorker::PersistentWorker(QObject* parent) : QObject(parent) {}

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
            m_cancel = false;
        }

        // Check cancellation before starting
        if (QThread::currentThread()->isInterruptionRequested()) {
            emit resultReady(job.id, "Cancelled", "err");
            continue;
        }


        static QRegularExpression factRe(R"(^\s*(?:fact\s*\(\s*(\d+)\s*\)|(\d+)\s*!)\s*$)");
        auto m = factRe.match(job.expression);
        if (m.hasMatch()) {
            QString nStr = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
            BigInt n(nStr.toStdString());
            if (n < 0) {
                emit resultReady(job.id, "Negative factorial", "err");
                continue;
            }
            try {
                auto callback = [this, jobId = job.id](int percent) {
                    emit progress(jobId, percent);
                    };
                QString result = BigNum::bigFactorial(n, callback);
                emit resultReady(job.id, result, "big");
            }
            catch (const std::exception& e) {
                emit resultReady(job.id, QString("Error: ") + e.what(), "err");
            }
            continue;
        }


        CalcResult res;
        try {
            res = MathEngine::evaluate(job.expression);
        }
        catch (const std::exception& e) {
            emit resultReady(job.id, QString("Error: ") + e.what(), "err");
            continue;
        }

        if (QThread::currentThread()->isInterruptionRequested()) {
            emit resultReady(job.id, "Cancelled", "err");
        }
        else {
            emit resultReady(job.id, res.result, res.type);
        }
    }
}