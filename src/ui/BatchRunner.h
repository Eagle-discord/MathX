#pragma once
#include <QDialog>
#include <QStringList>
#include <QTimer>

class QPlainTextEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QCheckBox;
class QSpinBox;
class MainWindow;

// ---------------------------------------------------------------------------
//  BatchRunner
//
//  A modal-less dialog that lets the user paste or type a list of expressions
//  (one per line) and runs them through MainWindow::runExpressionInternally
//  one at a time, waiting for each job to finish before firing the next.
//
//  Integration:
//    1. Construct once and keep alive (or make a member of MainWindow).
//    2. Call show() / raise() from a toolbar button / menu action.
//    3. Connect MainWindow's onWorkerFinish signal to BatchRunner::onJobDone
//       so the runner knows when to fire the next expression.
//
//  Example wiring in MainWindow::setupUi() or constructor:
//
//    m_batchRunner = new BatchRunner(this);
//    // tell the batch runner each time a job finishes
//    connect(this, &MainWindow::jobFinished, m_batchRunner, &BatchRunner::onJobDone);
//    // batch runner asks MainWindow to run an expression
//    connect(m_batchRunner, &BatchRunner::runExpression,
//            this, &MainWindow::runExpressionInternally);
//
//  You need to add one signal to MainWindow:
//    signals:
//        void jobFinished();   // emit at end of onWorkerFinish()
// ---------------------------------------------------------------------------
class BatchRunner : public QDialog {
    Q_OBJECT
public:
    explicit BatchRunner(QWidget* parent = nullptr);

public slots:
    /// Called by MainWindow when a worker job completes.
    void onJobDone();

signals:
    /// Emitted when the runner wants MainWindow to evaluate an expression.
    void runExpression(const QString& expr);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onClearClicked();
    void onLoadClicked();
    void onCopyResultsClicked();

private:
    // -- State machine --------------------------------------------------------
    enum class State { Idle, Running, Paused, Done };
    void setState(State s);
    void runNext();
    void finishBatch();
    QString buildResultsText() const;

    // -- UI helpers -----------------------------------------------------------
    void buildUi();
    void setRowHighlight(int row, const QString& color);
    void appendLog(const QString& msg, const QString& color = {});
    QString statusIcon(const QString& tag) const;

    // -- Data -----------------------------------------------------------------
    QStringList  m_expressions;   // parsed from the editor
    QStringList  m_results;       // result text per expression (populated as jobs finish)
    int          m_current = -1;  // index of the currently running expression
    int          m_doneCount = 0;
    int          m_errorCount = 0;
    State        m_state = State::Idle;

    // safety timeout - if a job takes longer than this, skip it
    QTimer* m_timeoutTimer = nullptr;
    static constexpr int TIMEOUT_MS = 30'000;

    // -- Widgets --------------------------------------------------------------
    QPlainTextEdit* m_editor = nullptr;   // expression input
    QPlainTextEdit* m_log = nullptr;   // live output log
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_loadBtn = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_statusLbl = nullptr;
    QLabel* m_countLbl = nullptr;
    QCheckBox* m_skipErrorsCb = nullptr;
    QSpinBox* m_delaySpinBox = nullptr;   // optional inter-job delay (ms)
};