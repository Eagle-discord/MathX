#pragma once
#include <QElapsedTimer>
#include <QWidget>
#include <QStringList>
#include <QTimer>
#include "../constants/ResultTypes.h"

class QPlainTextEdit;
class QPushButton;
class QLabel;
class QProgressBar;
class QCheckBox;
class QSpinBox;
class QPropertyAnimation;

class BatchPanel : public QWidget {
    Q_OBJECT
public:
    explicit BatchPanel(QWidget* parent = nullptr);

    bool isOpen()    const { return m_open; }
    bool isRunning() const { return m_running; }

    // Open the panel with `text` in the editor, appending rather than replacing
    // if the user already has a batch drafted. Used when a multi-line paste
    // arrives in the single-line input: QLineEdit silently flattens newlines, so
    // "f(x) = x*5\nf(2)" was being submitted as one string and failing with
    // "Unsupported equation type". The lines have to go somewhere that can hold
    // them, and that is here.
    //
    // Deliberately does NOT run anything -- the user still presses RUN. Pasting
    // is not consent to execute.
    void openWith(const QString& text);

public slots:
    void toggle();
    void onJobDone(const QString& result, ResultType type);

signals:
    void runExpression(const QString& expr);
    void openStateChanged(bool open);
    void batchFinished(int total, int errors);
    // New signals for MainWindow to hook into
    void batchStarted();
    // Carries the wall-clock ms from the first expression to the last, so
    // MainWindow can print it in the end-of-batch heading.
    void batchEnded(double elapsedMs);

private slots:
    void onRunClicked();
    void onStopClicked();
    void onClearClicked();

private:
    void buildUi();
    void startBatch();
    void runNext();
    void finishBatch();
    void animateOpen();
    void animateClose(std::function<void()> onDone = nullptr);
    QStringList parseExpressions() const;
    static QString colorForType(ResultType type);
    static QString iconForType(ResultType type);

    bool        m_open = false;
    bool        m_running = false;
    bool        m_stopRequested = false;

    QStringList m_expressions;
    QStringList m_results;
    int         m_current = -1;
    QElapsedTimer m_batchClock;   // wall-clock for the whole run
    int         m_done = 0;
    int         m_errors = 0;

    QTimer* m_timeoutTimer = nullptr;
    static constexpr int TIMEOUT_MS = 30'000;

    // Widgets
    QPlainTextEdit* m_editor = nullptr;
    QPushButton* m_runBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_clearBtn = nullptr;
    QPushButton* m_copyBtn = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_countLbl = nullptr;
    QLabel* m_statusLbl = nullptr;
    QSpinBox* m_delayBox = nullptr;
    QCheckBox* m_skipErrCb = nullptr;

    QPropertyAnimation* m_heightAnim = nullptr;
    int m_expandedHeight = 260;
};