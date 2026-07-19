#include "BatchRunner.h"
#include "../constants/Theme.h"
#include <qelapsedtimer.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFrame>
#include <QSplitter>
#include <QScrollBar>

// ---------------------------------------------------------------------------
//  Helpers
// ---------------------------------------------------------------------------
static QFont MF(int pt, int w = QFont::Normal) {
    return Theme::monoFont(pt, w);
}

static QPushButton* makeBtn(const QString& label, int h = 28) {
    auto* b = new QPushButton(label);
    b->setFont(MF(9));
    b->setFixedHeight(h);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

// ---------------------------------------------------------------------------
//  Construction
// ---------------------------------------------------------------------------
BatchRunner::BatchRunner(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Batch Runner");
    setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint);
    setAttribute(Qt::WA_DeleteOnClose, false);   // keep alive, just hide on close
    resize(760, 560);
    setMinimumSize(580, 420);

    setStyleSheet(QString(
        "QDialog,QWidget { background:%1; color:%2; }"
        "QPlainTextEdit  { background:%3; border:1px solid %4;"
        "                  border-radius:6px; color:%2; }"
        "QScrollBar:vertical { background:transparent; width:4px; margin:0; }"
        "QScrollBar::handle:vertical { background:%4; border-radius:2px; min-height:20px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
        "QCheckBox { color:%2; }"
        "QSpinBox  { background:%3; border:1px solid %4; color:%2;"
        "            border-radius:4px; padding:2px 6px; }"
    ).arg(Theme::BG, Theme::TEXT, Theme::SURFACE, Theme::BORDER));

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        appendLog(QString("  ⏱ Timeout after %1 s — skipping").arg(TIMEOUT_MS / 1000), Theme::WARN);
        m_results.append("[timeout]");
        onJobDone();   // treat as finished and move on
        });

    buildUi();
    setState(State::Idle);
}

// ---------------------------------------------------------------------------
//  UI construction
// ---------------------------------------------------------------------------
void BatchRunner::buildUi() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 14);
    root->setSpacing(10);

    // -- Title bar -----------------------------------------------------------
    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel("BATCH RUNNER");
    title->setFont(MF(11, QFont::Bold));
    title->setStyleSheet(QString("color:%1; letter-spacing:2px;").arg(Theme::ACCENT()));

    m_statusLbl = new QLabel("Ready");
    m_statusLbl->setFont(MF(9));
    m_statusLbl->setStyleSheet(QString("color:%1;").arg(Theme::MUTED));

    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(m_statusLbl);
    root->addLayout(titleRow);

    // -- Separator -----------------------------------------------------------
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("background:%1; border:none;").arg(Theme::BORDER));
    sep->setFixedHeight(1);
    root->addWidget(sep);

    // -- Main split: editor (left) | log (right) -----------------------------
    auto* splitter = new QSplitter(Qt::Horizontal);
    splitter->setHandleWidth(6);
    splitter->setStyleSheet(QString(
        "QSplitter::handle { background:%1; }"
    ).arg(Theme::BORDER));

    // Left panel — expression editor
    auto* leftWidget = new QWidget;
    auto* leftL = new QVBoxLayout(leftWidget);
    leftL->setContentsMargins(0, 0, 4, 0);
    leftL->setSpacing(6);

    auto* editorLabel = new QLabel("Expressions  (one per line, # comments ignored)");
    editorLabel->setFont(MF(8));
    editorLabel->setStyleSheet(QString("color:%1;").arg(Theme::MUTED));

    m_editor = new QPlainTextEdit;
    m_editor->setFont(MF(9));
    m_editor->setPlaceholderText(
        "# Arithmetic\n"
        "2 + 2\n"
        "100 * 3.14\n"
        "# Geometry\n"
        "circle r=5\n"
        "# Conversion\n"
        "100 km to miles\n"
    );
    m_editor->setTabChangesFocus(true);

    // Load from file / clear editor buttons
    auto* editorBtnRow = new QHBoxLayout;
    m_loadBtn = makeBtn("Load file…");
    m_clearBtn = makeBtn("Clear");
    m_loadBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%1;"
        "border-radius:5px; padding:0 10px; }"
        "QPushButton:hover { border-color:%2; color:%2; }"
    ).arg(Theme::MUTED, Theme::ACCENT()));
    m_clearBtn->setStyleSheet(m_loadBtn->styleSheet());

    editorBtnRow->addWidget(m_loadBtn);
    editorBtnRow->addWidget(m_clearBtn);
    editorBtnRow->addStretch();

    leftL->addWidget(editorLabel);
    leftL->addWidget(m_editor, 1);
    leftL->addLayout(editorBtnRow);

    // Right panel — live log
    auto* rightWidget = new QWidget;
    auto* rightL = new QVBoxLayout(rightWidget);
    rightL->setContentsMargins(4, 0, 0, 0);
    rightL->setSpacing(6);

    auto* logLabel = new QLabel("Output");
    logLabel->setFont(MF(8));
    logLabel->setStyleSheet(QString("color:%1;").arg(Theme::MUTED));

    m_log = new QPlainTextEdit;
    m_log->setFont(MF(9));
    m_log->setReadOnly(true);
    m_log->setStyleSheet(m_editor->styleSheet());

    m_copyBtn = makeBtn("Copy results");
    m_copyBtn->setStyleSheet(m_loadBtn->styleSheet());

    auto* logBtnRow = new QHBoxLayout;
    logBtnRow->addStretch();
    logBtnRow->addWidget(m_copyBtn);

    rightL->addWidget(logLabel);
    rightL->addWidget(m_log, 1);
    rightL->addLayout(logBtnRow);

    splitter->addWidget(leftWidget);
    splitter->addWidget(rightWidget);
    splitter->setSizes({ 340, 380 });

    root->addWidget(splitter, 1);

    // -- Progress bar --------------------------------------------------------
    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFixedHeight(6);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(QString(
        "QProgressBar { background:%1; border:none; border-radius:3px; }"
        "QProgressBar::chunk { background:%2; border-radius:3px; }"
    ).arg(Theme::SURFACE, Theme::ACCENT()));
    root->addWidget(m_progress);

    // -- Bottom control row --------------------------------------------------
    auto* bottomRow = new QHBoxLayout;
    bottomRow->setSpacing(8);

    // Options
    m_skipErrorsCb = new QCheckBox("Skip errors");
    m_skipErrorsCb->setFont(MF(9));
    m_skipErrorsCb->setChecked(true);

    auto* delayLabel = new QLabel("Delay:");
    delayLabel->setFont(MF(9));
    delayLabel->setStyleSheet(QString("color:%1;").arg(Theme::MUTED));

    m_delaySpinBox = new QSpinBox;
    m_delaySpinBox->setRange(0, 5000);
    m_delaySpinBox->setValue(0);
    m_delaySpinBox->setSuffix(" ms");
    m_delaySpinBox->setFont(MF(9));
    m_delaySpinBox->setFixedWidth(80);
    m_delaySpinBox->setToolTip("Delay between jobs (0 = run immediately)");

    m_countLbl = new QLabel("0 / 0");
    m_countLbl->setFont(MF(9));
    m_countLbl->setStyleSheet(QString("color:%1;").arg(Theme::MUTED));

    // Run / Stop
    m_startBtn = makeBtn("▶  Run Batch", 34);
    m_startBtn->setFixedWidth(120);
    m_startBtn->setStyleSheet(QString(
        "QPushButton { background:%1; border:none; color:#000;"
        "border-radius:6px; font-weight:bold; letter-spacing:1px; }"
        "QPushButton:hover { background:%2; }"
        "QPushButton:disabled { background:%3; color:%4; }"
    ).arg(Theme::ACCENT(), Theme::dimAccent(), Theme::SURFACE, Theme::MUTED));

    m_stopBtn = makeBtn("■  Stop", 34);
    m_stopBtn->setFixedWidth(90);
    m_stopBtn->setStyleSheet(QString(
        "QPushButton { background:%1; border:none; color:#fff;"
        "border-radius:6px; font-weight:bold; }"
        "QPushButton:hover { background:%2; }"
        "QPushButton:disabled { background:%3; color:%4; }"
    ).arg(Theme::ERR, Theme::DRED, Theme::SURFACE, Theme::MUTED));

    bottomRow->addWidget(m_skipErrorsCb);
    bottomRow->addSpacing(12);
    bottomRow->addWidget(delayLabel);
    bottomRow->addWidget(m_delaySpinBox);
    bottomRow->addStretch();
    bottomRow->addWidget(m_countLbl);
    bottomRow->addSpacing(8);
    bottomRow->addWidget(m_startBtn);
    bottomRow->addWidget(m_stopBtn);

    root->addLayout(bottomRow);

    // -- Signal wiring -------------------------------------------------------
    connect(m_startBtn, &QPushButton::clicked, this, &BatchRunner::onStartClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &BatchRunner::onStopClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &BatchRunner::onClearClicked);
    connect(m_loadBtn, &QPushButton::clicked, this, &BatchRunner::onLoadClicked);
    connect(m_copyBtn, &QPushButton::clicked, this, &BatchRunner::onCopyResultsClicked);
}

// ---------------------------------------------------------------------------
//  State machine
// ---------------------------------------------------------------------------
void BatchRunner::setState(State s) {
    m_state = s;
    switch (s) {
    case State::Idle:
        m_startBtn->setEnabled(true);
        m_startBtn->setText("▶  Run Batch");
        m_stopBtn->setEnabled(false);
        m_editor->setReadOnly(false);
        m_statusLbl->setText("Ready");
        m_statusLbl->setStyleSheet(QString("color:%1;").arg(Theme::MUTED));
        m_progress->setValue(0);
        break;

    case State::Running:
        m_startBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
        m_editor->setReadOnly(true);
        m_statusLbl->setText("Running…");
        m_statusLbl->setStyleSheet(QString("color:%1;").arg(Theme::ACCENT()));
        break;

    case State::Paused:
        m_startBtn->setEnabled(true);
        m_startBtn->setText("▶  Resume");
        m_stopBtn->setEnabled(true);
        m_statusLbl->setText("Paused");
        m_statusLbl->setStyleSheet(QString("color:%1;").arg(Theme::WARN));
        break;

    case State::Done:
        m_startBtn->setEnabled(true);
        m_startBtn->setText("▶  Run Again");
        m_stopBtn->setEnabled(false);
        m_editor->setReadOnly(false);
        m_progress->setValue(100);
        break;
    }
}

// ---------------------------------------------------------------------------
//  Start / Stop
// ---------------------------------------------------------------------------
void BatchRunner::onStartClicked() {
    if (m_state == State::Paused) {
        // Resume from where we left off
        setState(State::Running);
        runNext();
        return;
    }

    // Fresh run — parse the editor
    m_expressions.clear();
    m_results.clear();
    m_current = -1;
    m_doneCount = 0;
    m_errorCount = 0;
    m_log->clear();

    const QString raw = m_editor->toPlainText();
    for (const QString& line : raw.split('\n')) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith('#'))
            continue;
        // Strip inline comment
        int commentPos = trimmed.indexOf('#');
        if (commentPos > 0)
            trimmed = trimmed.left(commentPos).trimmed();
        if (!trimmed.isEmpty())
            m_expressions.append(trimmed);
    }

    if (m_expressions.isEmpty()) {
        appendLog("No expressions to run.", Theme::WARN);
        return;
    }

    m_results.resize(m_expressions.size());
    m_progress->setRange(0, m_expressions.size());
    m_progress->setValue(0);
    m_countLbl->setText(QString("0 / %1").arg(m_expressions.size()));

    appendLog(QString("-- Batch start: %1 expression(s)  %2 --")
        .arg(m_expressions.size())
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));

    setState(State::Running);
    runNext();
}

void BatchRunner::onStopClicked() {
    m_timeoutTimer->stop();
    if (m_state == State::Running) {
        // Mark remainder as skipped
        for (int i = m_current + 1; i < m_expressions.size(); ++i)
            m_results[i] = "[skipped]";
        appendLog("  ■ Stopped by user.", Theme::WARN);
        finishBatch();
    }
}

// ---------------------------------------------------------------------------
//  Core loop
// ---------------------------------------------------------------------------
void BatchRunner::runNext() {
    // BatchRunner::runNext, at the top:
    static QElapsedTimer wall;
    if (m_current == 0) wall.start();
    qDebug() << "line" << m_current << "wall:" << wall.elapsed() << "ms";
    ++m_current;

    if (m_current >= m_expressions.size()) {
        finishBatch();
        return;
    }

    const QString& expr = m_expressions[m_current];
    appendLog(QString("\n[%1/%2]  >> %3")
        .arg(m_current + 1)
        .arg(m_expressions.size())
        .arg(expr));

    m_timeoutTimer->start(TIMEOUT_MS);

    // Fire the expression into MainWindow — result comes back via onJobDone()
    emit runExpression(expr);
}

void BatchRunner::onJobDone() {
    // Guard: only process if we're actively running
    if (m_state != State::Running) return;
    // Guard: called once per job — double-calls are silently ignored
    if (m_current < 0 || m_current >= m_expressions.size()) return;

    m_timeoutTimer->stop();
    ++m_doneCount;

    int pct = (m_doneCount * 100) / m_expressions.size();
    m_progress->setValue(m_doneCount);
    m_countLbl->setText(QString("%1 / %2").arg(m_doneCount).arg(m_expressions.size()));

    // If a delay is set, wait before firing the next job
    int delay = m_delaySpinBox->value();
    if (delay > 0) {
        QTimer::singleShot(delay, this, [this]() {
            if (m_state == State::Running) runNext();
            });
    }
    else {
        QTimer::singleShot(0, this, [this]() {
            if (m_state == State::Running) runNext();
            });
    }
}

void BatchRunner::finishBatch() {
    m_timeoutTimer->stop();
    setState(State::Done);

    appendLog(QString("\n-- Batch done  %1 --  %2 OK, %3 error(s)")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(m_doneCount)
        .arg(m_errorCount));

    m_statusLbl->setText(QString("Done — %1 / %2").arg(m_doneCount).arg(m_expressions.size()));
    m_statusLbl->setStyleSheet(QString("color:%1;").arg(Theme::ACCENT()));
}

// ---------------------------------------------------------------------------
//  Log helpers
// ---------------------------------------------------------------------------
void BatchRunner::appendLog(const QString& msg, const QString& color) {
    // QPlainTextEdit doesn't support rich text, so we use plain text only.
    // Color is stored in the format tag but displayed as plain; for a
    // production version you'd swap QPlainTextEdit for QTextEdit with HTML.
    Q_UNUSED(color)
        m_log->appendPlainText(msg);
    // Auto-scroll
    m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

// ---------------------------------------------------------------------------
//  File actions
// ---------------------------------------------------------------------------
void BatchRunner::onLoadClicked() {
    const QString path = QFileDialog::getOpenFileName(
        this, "Load expressions", {}, "Text files (*.txt);;All files (*)");
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog("Failed to open file: " + path, Theme::ERR);
        return;
    }
    m_editor->setPlainText(QTextStream(&f).readAll());
    appendLog("Loaded: " + path);
}

void BatchRunner::onClearClicked() {
    if (m_state == State::Running) return;
    m_editor->clear();
    m_log->clear();
    m_expressions.clear();
    m_results.clear();
    m_current = -1;
    m_doneCount = 0;
    m_errorCount = 0;
    m_progress->setValue(0);
    m_countLbl->setText("0 / 0");
    setState(State::Idle);
}

void BatchRunner::onCopyResultsClicked() {
    QApplication::clipboard()->setText(buildResultsText());
    m_copyBtn->setText("Copied!");
    QTimer::singleShot(1500, this, [this]() { m_copyBtn->setText("Copy results"); });
}

QString BatchRunner::buildResultsText() const {
    QString out;
    for (int i = 0; i < m_expressions.size(); ++i) {
        out += QString(">> %1\n   %2\n\n")
            .arg(m_expressions.value(i))
            .arg(m_results.value(i, "(no result)"));
    }
    return out;
}