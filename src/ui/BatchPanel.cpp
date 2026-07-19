#include "BatchPanel.h"
#include "../constants/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QSpinBox>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QPropertyAnimation>
#include <QFrame>
#include "../constants/ResultTypes.h"

static QFont MF(int pt, int w = QFont::Normal) { return Theme::monoFont(pt, w); }

QString BatchPanel::colorForType(ResultType type) {
    if (type == ResultType::ok)   return Theme::ACCENT();
    if (type == ResultType::big)  return "#00d4ff";
    if (type == ResultType::err)  return Theme::ERR;
    if (type == ResultType::geo)  return Theme::INFO;
    if (type == ResultType::trig) return Theme::PURPLE;
    if (type == ResultType::conv) return Theme::WARN;
    if (type == ResultType::alg)  return Theme::ALG;
    return Theme::TEXT;
}

QString BatchPanel::iconForType(ResultType type) {
    if (type == ResultType::err) return "\xe2\x9c\x97";
    if (type == ResultType::geo) return "\xe2\x97\x88";
    return "\xe2\x9c\x93";
}

BatchPanel::BatchPanel(QWidget* parent) : QWidget(parent) {
    ;
    setVisible(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMaximumHeight(0);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    connect(m_timeoutTimer, &QTimer::timeout, this, [this]() {
        onJobDone("[timeout]", ResultType::err);
        });

    m_heightAnim = new QPropertyAnimation(this, "maximumHeight", this);
    m_heightAnim->setDuration(220);
    m_heightAnim->setEasingCurve(QEasingCurve::InOutCubic);

    buildUi();
}

void BatchPanel::buildUi() {
    setStyleSheet(QString(
        "BatchPanel { background:%1; border-top:1px solid %2; }"
    ).arg(Theme::SURFACE, Theme::BORDER));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(16, 10, 16, 10);
    root->setSpacing(8);

    // -- Header row ----------------------------------------------------------
    auto* headerRow = new QHBoxLayout;
    headerRow->setSpacing(10);

    auto* modeLbl = new QLabel("BATCH MODE");
    modeLbl->setFont(MF(8, QFont::Bold));
    modeLbl->setStyleSheet(QString(
        "color:%1; background:transparent; letter-spacing:2px;"
    ).arg(Theme::ACCENT()));

    m_statusLbl = new QLabel("Ready");
    m_statusLbl->setFont(MF(8));
    m_statusLbl->setStyleSheet(QString(
        "color:%1; background:transparent;"
    ).arg(Theme::MUTED));

    m_countLbl = new QLabel("0 / 0");
    m_countLbl->setFont(MF(8));
    m_countLbl->setStyleSheet(QString(
        "color:%1; background:transparent;"
    ).arg(Theme::MUTED));

    headerRow->addWidget(modeLbl);
    headerRow->addStretch();
    headerRow->addWidget(m_statusLbl);
    headerRow->addSpacing(12);
    headerRow->addWidget(m_countLbl);
    root->addLayout(headerRow);

    // -- Separator -----------------------------------------------------------
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1; border:none;").arg(Theme::BORDER));
    root->addWidget(sep);

    // -- Editor --------------------------------------------------------------
    m_editor = new QPlainTextEdit;
    m_editor->setFont(MF(9));
    m_editor->setFixedHeight(140);
    m_editor->setTabChangesFocus(true);
    m_editor->setStyleSheet(QString(
        "QPlainTextEdit {"
        "  background:%1; border:none; color:%2;"
        "}"
        "QScrollBar:vertical { background:transparent; width:4px; margin:0; }"
        "QScrollBar::handle:vertical { background:%3; border-radius:2px; min-height:16px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }"
    ).arg(Theme::SURFACE, Theme::TEXT, Theme::BORDER));
    m_editor->setPlaceholderText(
        "2 + 2\n"
        "sin(45)\n"
        "100 km to miles\n"
        "sphr(O) r=5\n"
        "fact(20)\n"
        "# lines starting with # are comments"
    );
    root->addWidget(m_editor);

    // -- Progress bar --------------------------------------------------------
    m_progress = new QProgressBar;
    m_progress->setRange(0, 100);
    m_progress->setValue(0);
    m_progress->setFixedHeight(3);
    m_progress->setTextVisible(false);
    m_progress->setStyleSheet(QString(
        "QProgressBar { background:%1; border:none; border-radius:1px; }"
        "QProgressBar::chunk { background:%2; border-radius:1px; }"
    ).arg(Theme::BORDER, Theme::ACCENT()));
    root->addWidget(m_progress);

    // -- Control row ---------------------------------------------------------
    auto* ctrlRow = new QHBoxLayout;
    ctrlRow->setSpacing(8);

    m_skipErrCb = new QCheckBox("Skip errors");
    m_skipErrCb->setFont(MF(8));
    m_skipErrCb->setChecked(true);
    m_skipErrCb->setStyleSheet(QString("color:%1;").arg(Theme::MUTED));

    auto* delayLbl = new QLabel("Delay:");
    delayLbl->setFont(MF(8));
    delayLbl->setStyleSheet(QString(
        "color:%1; background:transparent;"
    ).arg(Theme::MUTED));

    m_delayBox = new QSpinBox;
    m_delayBox->setRange(0, 5000);
    m_delayBox->setValue(0);
    m_delayBox->setSuffix(" ms");
    m_delayBox->setFont(MF(8));
    m_delayBox->setFixedWidth(72);
    m_delayBox->setStyleSheet(QString(
        "QSpinBox { background:%1; border:1px solid %2; color:%3;"
        "  border-radius:4px; padding:1px 4px; }"
    ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT));

    m_copyBtn = new QPushButton("Copy log");
    m_copyBtn->setFont(MF(8));
    m_copyBtn->setFixedHeight(22);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%1;"
        "  padding:0 8px; border-radius:4px; }"
        "QPushButton:hover { border-color:%2; color:%2; }"
    ).arg(Theme::MUTED, Theme::ACCENT()));
    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        QString out;
        for (int i = 0; i < m_expressions.size(); ++i) {
            out += QString(">> %1\n   %2\n\n")
                .arg(m_expressions.value(i))
                .arg(m_results.value(i, "(no result)"));
        }
        QApplication::clipboard()->setText(out);
        m_copyBtn->setText("Copied!");
        QTimer::singleShot(1400, this, [this]() { m_copyBtn->setText("Copy log"); });
        });

    m_clearBtn = new QPushButton("Clear");
    m_clearBtn->setFont(MF(8));
    m_clearBtn->setFixedHeight(22);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet(m_copyBtn->styleSheet());

    m_stopBtn = new QPushButton("STOP");
    m_stopBtn->setFont(MF(9, QFont::Bold));
    m_stopBtn->setFixedSize(64, 28);
    m_stopBtn->setCursor(Qt::PointingHandCursor);
    m_stopBtn->setEnabled(false);
    m_stopBtn->setStyleSheet(QString(
        "QPushButton { background:%1; border:none; color:#fff;"
        "  border-radius:6px; letter-spacing:1px; }"
        "QPushButton:hover { background:%2; }"
        "QPushButton:disabled { background:%3; color:%4; }"
    ).arg(Theme::ERR, Theme::DRED, Theme::SURFACE, Theme::MUTED));

    m_runBtn = new QPushButton("RUN");
    m_runBtn->setFont(MF(9, QFont::Bold));
    m_runBtn->setFixedSize(64, 28);
    m_runBtn->setCursor(Qt::PointingHandCursor);
    m_runBtn->setStyleSheet(QString(
        "QPushButton { background:%1; border:none; color:#000;"
        "  border-radius:6px; letter-spacing:1px; }"
        "QPushButton:hover { background:%2; }"
        "QPushButton:disabled { background:%3; color:%4; }"
    ).arg(Theme::ACCENT(), Theme::dimAccent(), Theme::SURFACE, Theme::MUTED));

    ctrlRow->addWidget(m_skipErrCb);
    ctrlRow->addSpacing(8);
    ctrlRow->addWidget(delayLbl);
    ctrlRow->addWidget(m_delayBox);
    ctrlRow->addStretch();
    ctrlRow->addWidget(m_copyBtn);
    ctrlRow->addSpacing(4);
    ctrlRow->addWidget(m_clearBtn);
    ctrlRow->addSpacing(8);
    ctrlRow->addWidget(m_stopBtn);
    ctrlRow->addWidget(m_runBtn);
    root->addLayout(ctrlRow);

    // Compute expanded height after layout is set
    adjustSize();
    m_expandedHeight = sizeHint().height();

    connect(m_runBtn, &QPushButton::clicked, this, &BatchPanel::onRunClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &BatchPanel::onStopClicked);
    connect(m_clearBtn, &QPushButton::clicked, this, &BatchPanel::onClearClicked);
}

// -- Animate open/close ------------------------------------------------------
void BatchPanel::animateOpen() {
    setVisible(true);
    m_heightAnim->stop();
    m_heightAnim->setStartValue(maximumHeight());
    m_heightAnim->setEndValue(m_expandedHeight);
    m_heightAnim->start();
}

void BatchPanel::animateClose(std::function<void()> onDone) {
    m_heightAnim->stop();
    m_heightAnim->setStartValue(maximumHeight());
    m_heightAnim->setEndValue(0);
    if (onDone) {
        QObject::connect(m_heightAnim, &QPropertyAnimation::finished,
            this, [this, onDone]() {
                setVisible(false);
                onDone();
                // Disconnect so it doesn't fire again next time
                QObject::disconnect(m_heightAnim, &QPropertyAnimation::finished,
                    this, nullptr);
            }, Qt::SingleShotConnection);
    }
    else {
        QObject::connect(m_heightAnim, &QPropertyAnimation::finished,
            this, [this]() {
                setVisible(false);
                QObject::disconnect(m_heightAnim, &QPropertyAnimation::finished,
                    this, nullptr);
            }, Qt::SingleShotConnection);
    }
    m_heightAnim->start();
}

// -- Toggle ------------------------------------------------------------------
void BatchPanel::openWith(const QString& text) {
    if (m_running) return;

    const QString existing = m_editor->toPlainText();
    if (existing.trimmed().isEmpty()) {
        m_editor->setPlainText(text);
    }
    else {
        // Append, don't replace. Someone with a batch already drafted who pastes
        // a couple more lines meant to ADD them; silently discarding their work
        // to make room for the paste would be the single worst thing this
        // feature could do.
        m_editor->setPlainText(
            existing + (existing.endsWith('\n') ? QString() : QStringLiteral("\n")) + text);
    }

    // Cursor to the end so the pasted lines are what's visible, and so typing
    // continues where the paste left off.
    m_editor->moveCursor(QTextCursor::End);

    if (!m_open) {
        m_open = true;
        emit openStateChanged(m_open);
        animateOpen();
    }
    m_editor->setFocus();
}

void BatchPanel::toggle() {
    if (m_running) return;

    m_open = !m_open;
    emit openStateChanged(m_open);

    if (m_open) {
        animateOpen();
        m_editor->setFocus();
    }
    else {
        animateClose();
    }
}

// -- Parse -------------------------------------------------------------------
QStringList BatchPanel::parseExpressions() const {
    QStringList out;
    for (const QString& raw : m_editor->toPlainText().split('\n')) {
        QString t = raw.trimmed();
        if (t.isEmpty() || t.startsWith('#') || t.startsWith("//")) continue;
        int ci = t.indexOf('#');
        if (ci > 0) t = t.left(ci).trimmed();

        int ci_2 = t.indexOf("//");
        if (ci_2 > 0) t = t.left(ci_2).trimmed();
        if (!t.isEmpty()) out.append(t);
    }
    return out;
}

// -- Run / Stop / Clear ------------------------------------------------------
void BatchPanel::onRunClicked() {
    m_expressions = parseExpressions();
    if (m_expressions.isEmpty()) return;
    startBatch();
}

void BatchPanel::startBatch() {
    m_results.clear();
    m_results.resize(m_expressions.size());
    m_current = -1;
    m_done = 0;
    m_errors = 0;
    m_stopRequested = false;

    m_progress->setRange(0, m_expressions.size());
    m_progress->setValue(0);
    m_countLbl->setText(QString("0 / %1").arg(m_expressions.size()));

    m_runBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_editor->setReadOnly(true);
    m_running = true;

    m_statusLbl->setText("Running\xe2\x80\xa6");
    m_statusLbl->setStyleSheet(QString(
        "color:%1; background:transparent;"
    ).arg(Theme::ACCENT()));

    // Animate panel closed, then start running
    // Results will show in terminal with heading
    animateClose([this]() {
        // Start the clock HERE, not at the top of startBatch: the close
        // animation runs first and is not part of the work being timed.
        m_batchClock.start();
        emit batchStarted(); // MainWindow adds "--- Batch ---" heading
        runNext();
        });
}

void BatchPanel::onStopClicked() {
    m_stopRequested = true;
    m_timeoutTimer->stop();
    finishBatch();
}

void BatchPanel::onClearClicked() {
    if (m_running) return;
    m_editor->clear();
    m_expressions.clear();
    m_results.clear();
    m_current = -1;
    m_done = m_errors = 0;
    m_progress->setValue(0);
    m_countLbl->setText("0 / 0");
    m_statusLbl->setText("Ready");
    m_statusLbl->setStyleSheet(QString(
        "color:%1; background:transparent;"
    ).arg(Theme::MUTED));
}

// -- Core loop ---------------------------------------------------------------
void BatchPanel::runNext() {
    ++m_current;
    if (m_stopRequested || m_current >= m_expressions.size()) {
        finishBatch();
        return;
    }
    m_timeoutTimer->start(TIMEOUT_MS);
    emit runExpression(m_expressions[m_current]);
}

void BatchPanel::onJobDone(const QString& result, ResultType type) {
    if (!m_running) return;
    if (m_current < 0 || m_current >= m_expressions.size()) return;

    m_timeoutTimer->stop();
    m_results[m_current] = result;

    if (type == ResultType::err) ++m_errors;
    ++m_done;

    m_progress->setValue(m_done);
    m_countLbl->setText(QString("%1 / %2").arg(m_done).arg(m_expressions.size()));

    if (type == ResultType::err && !m_skipErrCb->isChecked()) {
        finishBatch();
        return;
    }

    int delay = m_delayBox->value();
    if (delay > 0)
        QTimer::singleShot(delay, this, [this]() { if (m_running) runNext(); });
    else
        runNext();
}

void BatchPanel::finishBatch() {
    m_timeoutTimer->stop();
    m_running = false;
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_editor->setReadOnly(false);

    m_statusLbl->setText(m_errors > 0
        ? QString("Done \xe2\x80\x94 %1 error(s)").arg(m_errors)
        : "Done");
    m_statusLbl->setStyleSheet(QString("color:%1; background:transparent;")
        .arg(m_errors > 0 ? Theme::WARN : Theme::ACCENT()));

    m_progress->setValue(m_done);
    // nsecsElapsed keeps sub-millisecond resolution: a 5-line batch of trivial
    // arithmetic finishes in single-digit ms and "0 ms" would be a lie.
    const double elapsedMs = m_batchClock.isValid()
        ? m_batchClock.nsecsElapsed() / 1.0e6
        : 0.0;
    emit batchFinished(m_done, m_errors);
    emit batchEnded(elapsedMs); // MainWindow adds "--- end of batch ---" then re-opens panel

    // Re-open panel smoothly after batch ends
    m_open = true;
    animateOpen();
    emit openStateChanged(true);
}