#include "MainWindow.h"

#include "Animations.h"
#include "BatchPanel.h"
#include "BatchRunner.h"
#include "ClickableLabel.h"
#include "CopyableLabel.h"
#include "CRTTextLabel.h"
#include "FeatureViewerPage.h"
#include "GeoModeWidget.h"
#include "ModdedBtn.h"
#include "OutputArea.h"
#include "OutputWidget.h"
#include "SessionPanel.h"
#include "SidebarPanel.h"
#include "WidgetEditorPage.h"
#include "settings/SettingsPage.h"

#include "../constants/StateUtils.h"
#include "../constants/Theme.h"
#include "../constants/Duration.h"
#include "../input/InputHandler.h"
#include "../input/InputRouter.h"
#include "../input/Completer.h"
#include "../math/MathEngine.h"
#include "../settings/Settings.h"
#include "../shapes/GeoCard.h"
#include "../subSystems/WidgetRegistry.h"

#include <QApplication>
#include <QClipboard>
#include <QGraphicsDropShadowEffect>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QTimer>
#include <cmath>

static QFont MF(int pt = -1, int w = QFont::Normal) {
    return Theme::monoFont(pt > 0 ? pt : Theme::fontSize(), w);
}

// -----------------------------------------------------------------------------
//  Construction
// -----------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("MATHX \u2014 Unlimited Calculator"));
    setMinimumSize(900, 640);
    resize(1100, 750);
    setStyleSheet(QString("QMainWindow,QWidget{background:%1;color:%2;}").arg(C_BG, C_TEXT));

    setupUi();
    wireSettings();
    wireWorker();
    wireBatch();

    m_input->setFocus();
}

MainWindow::~MainWindow() {
    if (m_worker) m_worker->stop();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(5000);
        m_worker->deleteLater();
        m_workerThread->deleteLater();
    }
}

void MainWindow::wireSettings() {
    auto& S = Settings::instance();
    // WidgetRegistry handles every registered widget; these only exist because
    // the window-level stylesheet isn't owned by the registry.
    connect(&S, &Settings::fontSizeChanged, this, [this](int) { applySettings(); });
    connect(&S, &Settings::fontFamilyChanged, this, [this](const QString&) { applySettings(); });
    connect(&S, &Settings::accentColorChanged, this, [this](const QString&) { applySettings(); });
    connect(&S, &Settings::themeChanged, this, [this](const QString&) { applySettings(); });
    connect(&S, &Settings::settingsReset, this, [this]() { applySettings(); });
}

void MainWindow::wireWorker() {
    m_workerThread = new QThread(this);
    m_worker = new PersistentWorker();
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &PersistentWorker::process);
    connect(m_worker, &PersistentWorker::resultReady, this, &MainWindow::onWorkerFinish);
    // NOTE: this connection is effectively dead and kept only so the signal has
    // a receiver. m_worker lives on m_workerThread, so this is a QUEUED
    // connection -- but the worker never runs its event loop (process() blocks
    // in `while (!m_stop)` on m_cond for the app's lifetime), so the invocation
    // would only be delivered at shutdown. The Stop button does NOT rely on it:
    // MainWindow::onStop() calls m_worker->cancelAll() directly, which is safe
    // because cancelAll touches only an atomic and a mutex-guarded queue.
    connect(this, &MainWindow::stop, m_worker, &PersistentWorker::cancelAll);

    connect(m_worker, &PersistentWorker::progress, this,
        [this](int /*jobId*/, int percent, const QString& label) {
            m_output->showProgress(percent, label);
        });

    connect(m_output, &OutputArea::shapeProjectionRequested,
        this, &MainWindow::onShowGeometryMode);
    connect(m_output, &OutputArea::widgetEditRequested,
        this, &MainWindow::onShowWidgetEditor);
    connect(m_output, &OutputArea::expressionRecomputeRequested,
        this, &MainWindow::onExpressionRecompute);

    m_workerThread->start();
}

void MainWindow::wireBatch() {
    m_batchRunner = new BatchRunner(this);
    connect(this, &MainWindow::jobFinished, m_batchRunner, &BatchRunner::onJobDone);
    connect(this, &MainWindow::jobFinished, m_batchPanel, &BatchPanel::onJobDone);
    connect(m_batchPanel, &BatchPanel::runExpression,
        this, &MainWindow::runExpressionInternally);

    // openStateChanged fires when the PANEL opens/closes. batchStarted fires
    // when a batch RUNS. These are different events; the old code wired the
    // input-row visibility to the wrong one and papered over it with a second,
    // duplicate clicked() handler that toggled the panel twice per click.
    connect(m_batchPanel, &BatchPanel::openStateChanged, this, [this](bool open) {
        m_input->setVisible(!open);
        m_promptLbl->setVisible(!open && m_promptCtrl->isActive());
        m_runBtn->setVisible(!open && m_state != RunState::Running);
        m_stopBtn->setVisible(!open && m_state == RunState::Running);
        });

    connect(m_batchPanel, &BatchPanel::batchStarted, this, [this]() {
        const QString heading = QStringLiteral("\u2500\u2500\u2500 Batch \u2500\u2500\u2500");
        m_batchRunning = true;
        m_copyHistBuffer.append(heading + '\n');
        m_output->addBatchHeading(heading);
        });

    connect(m_batchPanel, &BatchPanel::batchEnded, this, [this](double elapsedMs) {
        const QString heading =
            QStringLiteral("\u2500\u2500\u2500 end of batch \u2014 %1 \u2500\u2500\u2500")
            .arg(Duration::fmt(elapsedMs));
        m_batchRunning = false;
        m_copyHistBuffer.append(heading + '\n');
        m_output->addBatchHeading(heading);
        });
}

void MainWindow::applySettings() {
    setStyleSheet(QString(
        "QMainWindow,QWidget{background:%1;color:%2;font-family:'%3';}"
    ).arg(C_BG, Theme::TEXT, Theme::fontFamily()));

    WidgetRegistry::instance().refreshAll();
}

// -----------------------------------------------------------------------------
//  Worker results
// -----------------------------------------------------------------------------
void MainWindow::appendTiming(double calcTime) {
    m_copyHistBuffer.append(Duration::calculatedIn(calcTime) + '\n');
}

// A job tagged in m_retargetJobs came from a scrubbed or inline-edited input
// line. Rewrite the EXISTING result widget and return: no addResultLine, no
// separator, no copy-history append, no counter bump, no jobFinished.
void MainWindow::handleRetarget(int jobId, const QString& result,
    ResultType type, double calcTime) {
    QPointer<CopyableLabel> target = m_retargetJobs.take(jobId);
    if (!target.isNull())
        m_output->updateResultLine(target.data(), result, type, calcTime);

    m_lastResult = result;
    m_sessionPanel->setLastResult(result);
    finishJob();
}

void MainWindow::handleGeoResult(const QString& expr, double calcTime) {
    if (GeoCard* card = InputHandler::makeGeoCard(expr)) {
        m_output->addGeoCard(card, calcTime);
        m_copyHistBuffer.append(card->buildCopyText() + "\n\n");
        appendTiming(calcTime);
        m_sessionPanel->setLastResult(QStringLiteral("Geometry Card"));
        m_output->addSeparator();
        return;
    }

    // Named a shape but stopped short: resume collecting the missing params.
    const ShapePrompt prompt = InputHandler::promptForMissingParams(expr);
    if (prompt.isActive()) {
        // ...unless this came from the batch runner. A prompt waits for someone
        // to type; in a batch nobody is there, so the run wedged until the 30 s
        // timeout fired and skipped the line (the caller, onWorkerFinish, skips
        // `emit jobFinished` while a prompt is active -- deliberately, so an
        // interactive prompt doesn't advance the batch under the user's feet).
        // Decline with a real result instead: no prompt is started, so the
        // caller's normal emit path runs and the batch advances immediately.
        if (m_batchRunning) {
            const QString missing = prompt.params.mid(prompt.currentIndex).join(", ");
            m_output->addResultLine(
                QString("'%1' needs %2 — batch mode can't prompt. "
                    "Try \"%1 %3 = ...\"")
                .arg(prompt.command, missing, prompt.nextParam()),
                ResultType::err, "", "", "", calcTime);
            appendTiming(calcTime);
            m_output->addSeparator();
            return;   // prompt NOT started -> onWorkerFinish emits jobFinished
        }
        setRunState(RunState::HandlingInput);
        m_promptCtrl->start(prompt);
        m_output->addPromptRequest(m_promptCtrl->currentParam());
        return;   // deliberately no finishJob(): we're mid-prompt
    }

    m_output->addResultLine("Invalid shape expression", ResultType::err, "", "", "", calcTime);
    appendTiming(calcTime);
    m_output->addSeparator();
}

void MainWindow::onWorkerFinish(int jobId, const QString& result, ResultType type,
    const QString& formula, double calcTime) {
    const QString expr = m_pendingJobs.take(jobId);

    if (m_retargetJobs.contains(jobId)) {
        handleRetarget(jobId, result, type, calcTime);
        return;
    }

    // Three mutually exclusive branches. The old code wrote these as two bare
    // `if`s and one `else if` chained to the second -- correct only by accident,
    // and one edit away from double-appending every big result.
    if (type == ResultType::geo) {
        handleGeoResult(expr, calcTime);

        // handleGeoResult may have entered HandlingInput to collect params.
        // Don't emit jobFinished or drop to Idle underneath an active prompt.
        if (m_promptCtrl->isActive()) return;
    }
    else {
        // Truncation for oversized results lives in OutputArea::displayTextFor,
        // reached via addResultLine. It used to be duplicated here, which meant
        // the scrub and retarget paths bypassed it and dumped 1786 digits into
        // a label.
        static int n = 0;
        QElapsedTimer t; t.start();
        m_output->addResultLine(result, type, result, formula, expr, calcTime);
        qDebug() << "line" << n++ << ":" << t.nsecsElapsed() / 1e6 << "ms";
        m_copyHistBuffer.append(result + "\n\n");
        appendTiming(calcTime);
        m_sessionPanel->setLastResult(result);
        m_output->addSeparator();
    }

    m_lastResult = result;
    finishJob();
    emit jobFinished(result, type);   // tells BatchRunner / BatchPanel the job is done
}

void MainWindow::finishJob() {
    if (m_output->progressState()) m_output->hideProgress();
    if (!m_promptCtrl->isActive()) setRunState(RunState::Idle);
}

// -----------------------------------------------------------------------------
//  Pipeline
// -----------------------------------------------------------------------------
void MainWindow::submitExpression(const QString& expr) {
    const bool fromApp = (StateUtils::getRunSource() == RunSource::App) && !m_batchRunning;

    if (!fromApp) {
        m_output->addInputLine(expr);
        m_sessionPanel->setLastExpression(expr);
        m_sessionPanel->setCalcCount(++m_calcCount);
        m_histNav->reset();
    }
    run(expr);
}

void MainWindow::run(const QString& expr) {
    m_copyHistBuffer.append(">> " + expr + '\n');
    if (!m_batchRunning) m_history.append(expr);

    m_sessionPanel->setLastExpression(expr);
    setRunState(RunState::Running);

    const int id = ++m_nextJobId;
    m_pendingJobs[id] = expr;
    m_worker->submitJob(id, expr);
}

void MainWindow::runExpressionInternally(const QString& expr) {
    // Set source before entering the pipeline so downstream guards can read
    // StateUtils::getRunSource() without an extra parameter. (This used to
    // call connect() on every invocation -- once per batch line.)
    StateUtils::setRunSource(RunSource::App);
    submitExpression(expr);
    StateUtils::setRunSource(RunSource::User);
}

void MainWindow::onExpressionRecompute(const QString& expr, CopyableLabel* target) {
    if (!target) return;
    if (m_state == RunState::Running) return;   // don't stack jobs mid-run

    const int id = ++m_nextJobId;
    m_pendingJobs[id] = expr;
    m_retargetJobs[id] = target;                // marks this job as in-place

    setRunState(RunState::Running);
    m_worker->submitJob(id, expr);
}

void MainWindow::onRun() {
    // Clicking RUN is intent to use the app, so the reel yields and its idle
    // countdown restarts. It can no longer corrupt anything either way: the demo
    // paints into the GHOST, so text() is empty for its entire run and the
    // isEmpty() check below already handles "RUN pressed while only a demo line
    // was showing". (The earlier setText-based version would have submitted the
    // demo's half-typed line as if the user wrote it. Gone by construction.)
    if (m_demo) m_demo->interrupted();

    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;

    StateUtils::setRunSource(RunSource::User);
    m_input->clear();

    if (m_promptCtrl->isActive()) {
        setRunState(RunState::HandlingInput);
        handlePromptInput(text);
        return;
    }
    if (!tryStartPrompt(text)) {
        submitExpression(text);
    }
}

void MainWindow::onStop() {
    if (m_promptCtrl && m_promptCtrl->isActive()) m_promptCtrl->cancel();
    if (m_worker) m_worker->cancelAll();
    setRunState(RunState::Stopping);
}

void MainWindow::onCopyHistory() {
    QApplication::clipboard()->setText(m_copyHistBuffer.join(QString()));
}

void MainWindow::onClear() {
    m_history.clear();
    m_copyHistBuffer.clear();
    m_histNav->reset();
    m_promptCtrl->reset();
    m_retargetJobs.clear();

    m_calcCount = 0;
    m_lastResult.clear();
    m_sessionPanel->reset();

    m_output->clearAll();
    m_input->clear();
    setRunState(RunState::Idle);
}

// -----------------------------------------------------------------------------
//  Run state
// -----------------------------------------------------------------------------
void MainWindow::setRunState(RunState state) {
    m_state = state;
    const bool batchOpen = m_batchPanel && m_batchPanel->isOpen();

    switch (state) {
    case RunState::Idle:
        m_stopBtn->hide();
        m_runBtn->setVisible(!batchOpen);
        m_input->setEnabled(true);
        if (!m_promptCtrl->isActive())
            m_input->setPlaceholderText("Enter expression, equation, or shape...");
        m_input->setFocus();
        Settings::instance().applyPending(true);
        break;

    case RunState::HandlingInput:
        m_runBtn->hide();
        m_stopBtn->setVisible(!batchOpen);
        m_input->setEnabled(true);
        break;

    case RunState::Running:
        m_runBtn->hide();
        m_stopBtn->setVisible(!batchOpen);
        m_input->setEnabled(false);
        break;

    case RunState::Stopping:
        emit stop();
        break;
    }
}

// -----------------------------------------------------------------------------
//  Prompts
// -----------------------------------------------------------------------------
bool MainWindow::tryStartPrompt(const QString& expr) {
    const ShapePrompt p = InputHandler::detectPrompt(expr);
    if (!p.isActive()) return false;

    setRunState(RunState::HandlingInput);
    m_promptCtrl->start(p);
    m_output->addPromptRequest(m_promptCtrl->currentParam());
    return true;
}

void MainWindow::handlePromptInput(const QString& value) {
    const QString param = m_promptCtrl->currentParam();
    bool ok = false;
    double numericValue = value.toDouble(&ok);

    if (!ok && !value.contains('=')) {
        bool evalOk = false;
        const double v = MathEngine::evalWith(value, m_promptCtrl->collectedVars(), evalOk);
        if (evalOk && std::isfinite(v)) {
            numericValue = v;
            ok = true;
            m_output->addResultLine(
                QString("  (evaluated '%1' \u2192 %2)")
                .arg(value, QString::number(v, 'g', 10)), ResultType::ok);
        }
    }

    if (!ok && value.contains('=')) {
        double solved = 0.0;
        if (MathEngine::solveEquation(value, solved)) {
            QMessageBox mb(this);
            mb.setWindowTitle("Equation Solved");
            mb.setText(QString("'%1' solves to %2 = %3").arg(value, param).arg(solved));
            mb.setInformativeText("Use this value?");
            mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            mb.setDefaultButton(QMessageBox::Yes);
            if (mb.exec() == QMessageBox::Yes) {
                numericValue = solved;
                ok = true;
            }
            else {
                m_output->addResultLine(
                    "Equation solution rejected. Please enter a number.", ResultType::err);
                m_output->addPromptRequest(param);
                m_input->setFocus();
                return;
            }
        }
    }

    if (!ok) {
        m_output->addResultLine(
            QString("\u26a0 Invalid input for '%1'").arg(param), ResultType::err);
        m_output->addPromptRequest(param);
        m_input->setFocus();
        return;
    }

    m_promptCtrl->submit(value, numericValue);
}

void MainWindow::onParamReady(const QString& param, const QString& value) {
    m_output->addPromptAnswer(param, value);
    if (m_promptCtrl->isActive())
        m_output->addPromptRequest(m_promptCtrl->currentParam());
}

void MainWindow::onPromptComplete(const QString& fullExpr) {
    m_output->addSeparator();
    run(fullExpr);
}

void MainWindow::onPromptCancelled() {
    m_output->addResultLine("  Input operation cancelled by user", ResultType::ok);
    m_output->addSeparator();
    setRunState(RunState::Idle);
}

// -----------------------------------------------------------------------------
//  Pages
// -----------------------------------------------------------------------------
void MainWindow::onShowGeometryMode(const QString& type, const QMap<QString, double>& params) {
    if (!m_geoModeWidget) {
        m_geoModeWidget = new GeoModeWidget;
        m_centralStack->addWidget(m_geoModeWidget);
        connect(m_geoModeWidget, &GeoModeWidget::backClicked,
            this, &MainWindow::destroyGeometryMode);
    }
    m_geoModeWidget->setShape(type, params);

    QLayout* root = centralWidget()->layout();
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    m_centralStack->setCurrentWidget(m_geoModeWidget);
}

void MainWindow::destroyGeometryMode() {
    if (!m_geoModeWidget) return;

    QLayout* root = centralWidget()->layout();
    root->setContentsMargins(m_originalMargins);
    root->setSpacing(m_originalSpacing);
    m_centralStack->setCurrentWidget(m_terminalPage);

    // deleteLater, not delete: we're inside a signal emitted by this widget.
    // The old code also called QApplication::processEvents() here, which invited
    // reentrancy on the very object it was about to destroy.
    m_centralStack->removeWidget(m_geoModeWidget);
    m_geoModeWidget->deleteLater();
    m_geoModeWidget = nullptr;
}

void MainWindow::onShowWidgetEditor(QWidget* target) {
    if (!m_widgetEditorPage) {
        m_widgetEditorPage = new WidgetEditorPage;
        m_centralStack->addWidget(m_widgetEditorPage);
        connect(m_widgetEditorPage, &WidgetEditorPage::backClicked, this, [this]() {
            m_centralStack->setCurrentWidget(m_terminalPage);
            });
    }
    m_widgetEditorPage->inspect(target);
    m_centralStack->setCurrentWidget(m_widgetEditorPage);
}

void MainWindow::destroyWidgetEditor() {
    if (!m_widgetEditorPage) return;
    m_centralStack->removeWidget(m_widgetEditorPage);
    m_widgetEditorPage->deleteLater();
    m_widgetEditorPage = nullptr;
}

// -----------------------------------------------------------------------------
//  Sidebar / modes
// -----------------------------------------------------------------------------
void MainWindow::onModeChanged(const QString& mode) {
    m_currentMode = mode;
    m_refPanel->renderMode(mode);
}

void MainWindow::onSidebarItemClicked(const QString& expr) {
    if (m_state != RunState::Idle) return;
    // Clicking an example is intent to use the app. The reel yields; its idle
    // countdown restarts and won't fire again while this text sits in the field.
    if (m_demo) m_demo->interrupted();
    m_input->setText(expr);
    m_input->setFocus();
}

void MainWindow::onSidebarItemDoubleClicked(const QString& expr) {
    if (m_state != RunState::Idle) return;
    if (m_demo) m_demo->interrupted();   // see onSidebarItemClicked
    StateUtils::setRunSource(RunSource::User);
    submitExpression(expr);
    m_input->clear();
    m_input->setFocus();
}

// -----------------------------------------------------------------------------
//  Events
// -----------------------------------------------------------------------------
bool MainWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj != m_input)
        return QMainWindow::eventFilter(obj, ev);

    // The user always wins the input field. Any real interaction yields the reel
    // immediately and pushes its idle countdown back; it only returns after
    // idleMs of nothing, and only over an empty field (see DemoTyper.h).
    //
    // Called even when the demo isn't running -- interrupted() is what RESTARTS
    // the countdown, so every keystroke postpones the next reel. It's two timer
    // restarts; cheap enough for a keypress handler.
    //
    // NOT FocusIn: MainWindow calls m_input->setFocus() from eight places
    // including its own constructor, so focus is not evidence of intent. A
    // keypress or a deliberate click is.
    if (m_demo) {
        switch (ev->type()) {
        case QEvent::KeyPress:
        case QEvent::MouseButtonPress:
        case QEvent::InputMethod:
            m_demo->interrupted();
            break;
        default:
            break;
        }
    }

    if (ev->type() != QEvent::KeyPress)
        return QMainWindow::eventFilter(obj, ev);

    // Order matters: interrupted() above already ran, and it clears the ghost IF
    // the demo owned it -- so hasGhost() is false here when the demo was
    // mid-line, and Tab correctly falls through to focus traversal instead of
    // "accepting" our text into the user's field. A real completion ghost is
    // untouched (interrupted() only clears while m_running), so Tab still works.
    auto* ke = static_cast<QKeyEvent*>(ev);
    const InputAction action = InputRouter::translate(
        ke, m_promptCtrl->isActive(), m_history.isEmpty(), m_input->hasGhost());

    switch (action) {
    case InputAction::CancelPrompt:
        m_promptCtrl->cancel();
        return true;
    case InputAction::Complete: {
        // Accept the ghost. Read it from the widget rather than recomputing:
        // recomputing risks accepting something different from what was painted
        // if any input to Completer changed since the last repaint.
        const QString full = m_input->text() + m_input->ghost();
        m_input->setText(full);          // textChanged -> ghost recomputed
        m_input->setCursorPosition(full.size());
        return true;
    }
    case InputAction::HistoryBack:
        m_histNav->saveTypedText(m_input->text());
        m_input->setText(m_histNav->back());
        return true;
    case InputAction::HistoryForward:
        m_input->setText(m_histNav->forward());
        return true;
    case InputAction::Submit:
        return false;   // let returnPressed handle it
    default:
        return QMainWindow::eventFilter(obj, ev);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_worker) m_worker->stop();
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    QMainWindow::closeEvent(event);
}

// -----------------------------------------------------------------------------
//  UI construction
// -----------------------------------------------------------------------------
void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    central->setStyleSheet(QString("background:%1;").arg(C_BG));
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(24, 20, 24, 24);
    root->setSpacing(16);
    m_originalMargins = root->contentsMargins();
    m_originalSpacing = root->spacing();

    m_centralStack = new QStackedWidget;
    m_centralStack->setStyleSheet("border:none;");

    m_terminalPage = new QWidget;
    auto* pageLayout = new QVBoxLayout(m_terminalPage);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->setSpacing(16);

    setupHeader();
    pageLayout->addWidget(m_header);

    auto* contentRow = new QWidget;
    auto* rowLayout = new QHBoxLayout(contentRow);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(16);

    setupTerminal();
    rowLayout->addWidget(m_terminal, 1);
    rowLayout->addWidget(createSidebar());

    pageLayout->addWidget(contentRow, 1);
    m_centralStack->addWidget(m_terminalPage);

    // GeoModeWidget, SettingsPage, WidgetEditorPage are lazily constructed.
    root->addWidget(m_centralStack, 1);
}

void MainWindow::setupHeader() {
    m_header = new QWidget;
    m_header->setStyleSheet("background:transparent;");
    m_header->setAttribute(Qt::WA_TranslucentBackground);
    m_header->setAutoFillBackground(false);

    auto* hl = new QHBoxLayout(m_header);
    hl->setContentsMargins(8, 0, 8, 0);
    hl->setSpacing(8);

    auto* logo = new CRTTextLabel("MATHX", this);
    logo->setSubtitle("UNLIMITED CALCULATOR", 9);
    logo->setGlowColor(QColor(0, 255, 65));
    logo->setGlowLayers(3);
    logo->setScanlineOpacity(80);
    //  logo->setFlickerInterval(20);
  //    logo->setFlickerEnabled(true);
    logo->loadFontFromFile(":/fonts/Orbitron-VariableFont_wght.ttf", 20);
    logo->setFixedSize(200, 48);

    hl->addWidget(logo, 0, Qt::AlignLeft | Qt::AlignVCenter);
    hl->addStretch(1);

    // -- Mode tabs -------------------------------------------------------------
    m_modesBar = new QWidget;
    m_modesBar->setStyleSheet("background:transparent;");
    auto* ml = new QHBoxLayout(m_modesBar);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(6);

    struct Mode { QString label, key; };
    static const QList<Mode> modes = {
        {"All","all"}, {"Arithmetic","arithmetic"}, {"Algebra","algebra"},
        {"Trigonometry","trigonometry"}, {"Geometry","geometry"}, {"Convert","conversion"}
    };

    // Built from Theme:: at click time, so a live accent change repaints correctly.
    auto inactiveCss = []() {
        return QString(
            "QPushButton{background:none;border:1px solid %1;color:%2;"
            "padding:2px 12px;border-radius:13px;letter-spacing:1px;}"
            "QPushButton:hover{border-color:%3;color:%3;}"
        ).arg(C_BORDER, Theme::MUTED, Theme::dimAccent());
        };
    auto activeCss = []() {
        return QString(
            "QPushButton{background:%1;border:1px solid %1;color:#000;"
            "padding:2px 12px;border-radius:13px;letter-spacing:1px;font-weight:bold;}"
        ).arg(Theme::ACCENT());
        };

    for (int i = 0; i < modes.size(); ++i) {
        auto* btn = new QPushButton(modes[i].label);
        btn->setProperty("modeKey", modes[i].key);
        btn->setFont(MF(8));
        btn->setFixedHeight(26);
        btn->setStyleSheet(i == 0 ? activeCss() : inactiveCss());
        if (i == 0) m_activeMode = btn;

        connect(btn, &QPushButton::clicked, this, [this, btn, inactiveCss, activeCss]() {
            if (m_activeMode && m_activeMode != btn)
                m_activeMode->setStyleSheet(inactiveCss());
            btn->setStyleSheet(activeCss());
            m_activeMode = btn;
            onModeChanged(btn->property("modeKey").toString());
            });
        ml->addWidget(btn);
    }

    hl->addWidget(m_modesBar, 0, Qt::AlignVCenter);
    hl->addSpacing(8);

    // -- Features button -------------------------------------------------------
    m_featuresBtn = new QPushButton(QStringLiteral("☰"));
    m_featuresBtn->setFixedSize(32, 32);
    m_featuresBtn->setFont(MF(13));
    m_featuresBtn->setCursor(Qt::PointingHandCursor);
    m_featuresBtn->setToolTip("Features");

    WR_ADD(m_featuresBtnId, m_featuresBtn, WidgetRole::FontUI);
    WidgetRegistry::instance().setStylesheet(m_featuresBtnId, QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;"
        "border-radius:6px;padding:0px;}"
        "QPushButton:hover{border-color:%3;color:%3;}"
    ).arg(C_BORDER, Theme::MUTED, Theme::ACCENT()));

    connect(m_featuresBtn, &QPushButton::clicked, this, [this]() {
        ensureFeaturePage();
        m_centralStack->setCurrentWidget(m_featurePage);
        });

    hl->addWidget(m_featuresBtn, 0, Qt::AlignVCenter);

    // -- Settings gear ---------------------------------------------------------
    m_settingsBtn = new QPushButton(QStringLiteral("\u2699"));
    m_settingsBtn->setFixedSize(32, 32);
    m_settingsBtn->setFont(MF(13));
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip("Settings");

    // m_settingsBtnId was declared but never assigned, so the gear was the one
    // header widget that didn't repaint on a theme change.
    WR_ADD(m_settingsBtnId, m_settingsBtn, WidgetRole::FontUI);
    WidgetRegistry::instance().setStylesheet(m_settingsBtnId, QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;"
        "border-radius:6px;padding:0px;}"
        "QPushButton:hover{border-color:%3;color:%3;}"
    ).arg(C_BORDER, Theme::MUTED, Theme::ACCENT()));

    connect(m_settingsBtn, &QPushButton::clicked, this, [this]() {
        ensureSettingsPage();
        m_centralStack->setCurrentWidget(m_settingsPage);
        m_settingsBtn->hide();
        m_settingsBackBtn->show();
        });

    hl->addWidget(m_settingsBtn, 0, Qt::AlignVCenter);
}

void MainWindow::setupTerminal() {
    m_terminal = new QFrame;
    m_terminal->setObjectName("terminal");
    m_terminal->setStyleSheet(QString(
        "QFrame#terminal{background:%1;border:1px solid %2;border-radius:12px;}"
    ).arg(C_SURFACE, C_BORDER));

    auto* tl = new QVBoxLayout(m_terminal);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(0);

    // -- Title bar -------------------------------------------------------------
    m_termBar = new QWidget;
    m_termBar->setObjectName("m_termBar");
    m_termBar->setFixedHeight(36);
    m_termBar->setStyleSheet(QString(
        "QWidget#m_termBar{background:%1;border-bottom:1px solid %2;"
        "border-top-left-radius:12px;border-top-right-radius:12px;}"
    ).arg(C_CARD, C_BORDER));

    auto* tbL = new QHBoxLayout(m_termBar);
    tbL->setContentsMargins(14, 2, 16, 2);
    tbL->setSpacing(7);

    for (const QString& col : { "#ff5f57", "#febc2e", "#28c840" }) {
        auto* dot = new QLabel;
        dot->setFixedSize(12, 12);
        dot->setStyleSheet(QString("background:%1;border-radius:6px;").arg(col));
        tbL->addWidget(dot);
    }

    m_termTitle = new QLabel(QStringLiteral("mathx v1.0 \u2014 session"), m_termBar);
    m_termTitle->setFont(MF(9));
    m_termTitle->setStyleSheet(
        QString("color:%1;background:transparent;letter-spacing:1px;").arg(C_MUTED));

    m_copyHistBtn = new QPushButton("Copy History");
    m_copyHistBtn->setFont(MF(10));
    m_copyHistBtn->setFixedSize(112, 19);

    tbL->addStretch();
    tbL->addWidget(m_termTitle, 0, Qt::AlignHCenter);
    tbL->addStretch();
    tbL->addWidget(m_copyHistBtn, 0, Qt::AlignRight);

    WR_ADD(m_termTitleId, m_termTitle, WidgetRole::Muted | WidgetRole::FontUI);
    WR_ADD(m_copyHistBtnId, m_copyHistBtn, WidgetRole::FontUI);

    const QString copyBtnCss = QString(
        "QPushButton{background:none;border:1px solid %1;color:%1;"
        "padding:1px 8px;border-radius:4px;}"
        "QPushButton:hover{border-color:%2;color:%2;}"
    ).arg(Theme::MUTED, Theme::ACCENT());
    WidgetRegistry::instance().setStylesheet(m_copyHistBtnId, copyBtnCss);

    connect(m_copyHistBtn, &QPushButton::clicked, this, [this]() {
        onCopyHistory();
        m_copyHistBtn->setStyleSheet(QString(
            "QPushButton{background:%1;border:none;color:#000;"
            "padding:1px 8px;border-radius:4px;}"
        ).arg(Theme::ACCENT()));
        m_copyHistBtn->setText("Copied!");
        QTimer::singleShot(1500, this, [this]() {
            m_copyHistBtn->setText("Copy History");
            WidgetRegistry::instance().refresh(m_copyHistBtnId);
            });
        });

    // -- Output ----------------------------------------------------------------
    m_output = new OutputArea;

    // -- Input row -------------------------------------------------------------
    m_inputRow = new QFrame;
    m_inputRow->setObjectName("inputRow");
    m_inputRow->setFixedHeight(52);
    m_inputRow->setStyleSheet(QString(
        "QFrame#inputRow{background:%1;border-top:1px solid %2;"
        "border-bottom-left-radius:12px;border-bottom-right-radius:12px;}"
    ).arg(C_CARD, C_BORDER));

    auto* ir = new QHBoxLayout(m_inputRow);
    ir->setContentsMargins(16, 0, 14, 0);
    ir->setSpacing(10);

    m_promptBtn = new ModdedBtn(">>");
    m_promptBtn->setFont(MF(12, QFont::Bold));
    m_promptBtn->setCursor(Qt::PointingHandCursor);
    m_promptBtn->setToolTip("Toggle batch mode");
    m_promptBtn->setStyleSheet(QString(
        "QPushButton{background:none;border:none;color:%1;}"
        "QPushButton:hover{color:%2;}"
    ).arg(C_ACCENT, Theme::dimAccent()));

    auto* glow = new QGraphicsDropShadowEffect(m_promptBtn);
    glow->setOffset(0, 0);
    glow->setBlurRadius(0);
    glow->setColor(QColor(C_ACCENT));
    m_promptBtn->setGraphicsEffect(glow);

    auto* glowAnim = new QPropertyAnimation(glow, "blurRadius", m_promptBtn);
    glowAnim->setDuration(120);
    connect(m_promptBtn, &ModdedBtn::hoverEnter, this, [glow, glowAnim]() {
        glowAnim->stop();
        glowAnim->setStartValue(glow->blurRadius());
        glowAnim->setEndValue(12);
        glowAnim->start();
        });
    connect(m_promptBtn, &ModdedBtn::hoverLeave, this, [glow, glowAnim]() {
        glowAnim->stop();
        glowAnim->setStartValue(glow->blurRadius());
        glowAnim->setEndValue(0);
        glowAnim->start();
        });

    m_batchPanel = new BatchPanel(this);

    m_promptLbl = new QLabel;
    m_promptLbl->setFont(MF(10));
    m_promptLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(C_ACCENT));
    m_promptLbl->hide();

    m_input = new GhostLineEdit(this);
    m_input->setPlaceholderText("Enter expression, equation, or shape...");
    m_input->setFont(MF(10));
    m_input->setFrame(false);
    m_input->setStyleSheet(
        QString("QLineEdit{background:transparent;border:none;color:%1;}").arg(C_TEXT));

    // Recompute the inline suggestion on every text change. Completer::ghost is
    // cheap (a prefix scan over history plus a few dozen identifiers) and this
    // fires per character, so it stays out of the way of typing.
    //
    // textChanged, not textEdited: textEdited fires only for user keystrokes, but
    // arrow-key history navigation and Tab acceptance both use setText(), and the
    // ghost must refresh for those too or it goes stale the moment you press Up.
    //
    // Suppressed during a shape prompt: there the field holds a parameter value,
    // not an expression, and suggesting "circle" while the user types a radius
    // would be actively wrong.
    connect(m_input, &QLineEdit::textChanged, this, [this](const QString& t) {
        // Suppressed during a shape prompt: the field holds a parameter value,
        // not an expression, and suggesting "circle" while the user types a
        // radius would be actively wrong.
        //
        // NOT guarded against the demo reel any more, and must not be: the demo
        // paints into the ghost, so this handler blanking the ghost would erase
        // the demo mid-word. It cannot conflict -- during the demo text() is
        // empty, so Completer::ghost("") returns "" and this is a no-op.
        //
        // Suppressed while the worker is running: Completer reads
        // MathEngine's variable/function registries, which are unguarded statics
        // the worker WRITES during evaluate(). Reading them mid-run is a data
        // race (see the note in Completer.h). Idle is the only safe window until
        // those registries get a mutex or a UI-side snapshot.
        if (m_demo && m_demo->isRunning()) return;   // demo owns the ghost
        const bool busy = (m_state == RunState::Running);
        if (busy || (m_promptCtrl && m_promptCtrl->isActive())) {
            m_input->setGhost(QString());
            return;
        }
        m_input->setGhost(Completer::ghost(t, m_history));
        });

    // -- Demo reel -------------------------------------------------------------
    // Types example expressions into the input bar's ghost layer, to show what
    // the app accepts without a manual. It never runs anything and never touches
    // history; text() stays empty for its entire run, which is what makes it
    // safe (see DemoTyper.h).
    //
    // Plays 5 s after launch, yields the instant the user does anything, and
    // returns after 5 s of no input -- but only while the field is EMPTY. An
    // empty field is the signal that nobody is mid-thought; resuming over a
    // half-typed expression would be hostile.
    //
    // Curated, not scraped from the sidebar: this is a trailer, so it wants
    // breadth (algebra / trig / geometry / units / bignum / natural language) in
    // the first few entries rather than depth in one category.
    m_demo = new DemoTyper(m_input, this);
    // 5 s on a fresh app; 30 s once the user has engaged. onRun() clears the
    // input, so a single 5 s window would start the reel five seconds after
    // every result -- while the user is still reading it.
    m_demo->setIdleTimeout(5000, 30000);
    m_demo->setExpressions({
        "2 + 3 * (4^2)",
        "88x = 704",
        "x^2 - 5x + 6 = 0",
        "sin(30) + cos(60)",
        "100 km to miles",
        "72 f to c",
        "tri a=3 b=4 c=5",
        "circ(O) r=7",
        "fact(50)",
        "2^1000",
        "half of 80",
        "what is 15 percent of 200",
        "f(x) = x^2 + 1",
        "x^x where x = 5",
        });
    m_demo->start(5000);

    // -- Multi-line paste -> batch mode ---------------------------------------
    // QLineEdit silently flattens newlines, so pasting
    //     f(x) = x*5
    //     f(2)
    // submitted "f(x) = x*5f(2)" and failed with "Unsupported equation type" --
    // a real bug, not a missing nicety. The lines have to go somewhere that can
    // hold them, and the batch editor already exists for exactly this shape of
    // input.
    //
    // Nothing runs: the panel opens pre-filled and the user still presses RUN.
    // Pasting is not consent to execute.
    m_input->setMultilinePasteHandler([this](const QString& text) -> bool {
        if (m_state == RunState::Running) return false;   // let it flatten; we're busy
        if (m_demo) m_demo->interrupted();
        m_batchPanel->openWith(text);
        pulsePromptBtn();
        return true;
        });

    m_runBtn = new QPushButton("RUN");
    m_runBtn->setFont(MF(9, QFont::Bold));
    m_runBtn->setFixedSize(72, 32);

    m_stopBtn = new QPushButton("STOP");
    m_stopBtn->setFont(MF(9, QFont::Bold));
    m_stopBtn->setFixedSize(72, 32);
    m_stopBtn->hide();

    WR_ADD(m_promptBtnId, m_promptBtn, WidgetRole::AccentText | WidgetRole::FontUI);
    WR_ADD(m_inputId, m_input, WidgetRole::Text | WidgetRole::FontInput);
    WR_ADD(m_runBtnId, m_runBtn, WidgetRole::AccentFill | WidgetRole::FontUI);
    WR_ADD(m_stopBtnId, m_stopBtn, WidgetRole::FontUI);
    WidgetRegistry::instance().setStylesheet(m_stopBtnId, QString(
        "QPushButton{background:%1;border:none;color:#fff;border-radius:6px;letter-spacing:1px;}"
        "QPushButton:hover{background:%2;}"
    ).arg(C_VRED, C_DRED));

    ir->addWidget(m_promptBtn);
    ir->addWidget(m_promptLbl);
    ir->addWidget(m_input, 1);
    ir->addWidget(m_runBtn);
    ir->addWidget(m_stopBtn);

    tl->addWidget(m_termBar);
    tl->addWidget(m_output, 1);
    tl->addWidget(m_inputRow);
    tl->addWidget(m_batchPanel);

    // -- Input subsystems ------------------------------------------------------
    m_histNav = new HistoryNavigator(m_history);
    m_promptCtrl = new PromptController(m_promptLbl, m_input, this);

    connect(m_runBtn, &QPushButton::clicked, this, &MainWindow::onRun);
    connect(m_input, &QLineEdit::returnPressed, this, &MainWindow::onRun);
    connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_input, &QLineEdit::textEdited, this, [this]() { m_histNav->reset(); });

    connect(m_promptCtrl, &PromptController::paramReady, this, &MainWindow::onParamReady);
    connect(m_promptCtrl, &PromptController::promptComplete, this, &MainWindow::onPromptComplete);
    connect(m_promptCtrl, &PromptController::promptCancelled, this, &MainWindow::onPromptCancelled);
    connect(m_promptCtrl, &PromptController::needsMoreInput, this,
        [this](const QString& param, const QString& reason) {
            m_output->addResultLine(QString("\u26a0 %1").arg(reason), ResultType::err);
            m_output->addPromptRequest(param);
            m_input->setFocus();
        });

    connect(MathEngine::instance(), &MathEngine::simplification,
        m_output, &OutputArea::addSimplified);

    // Exactly one clicked() handler. There used to be two, and the panel
    // toggled twice per press.
    connect(m_promptBtn, &QPushButton::clicked, this, [this]() {
        if (m_state == RunState::Running) return;
        m_batchPanel->toggle();
        });

    m_input->installEventFilter(this);
}

void MainWindow::pulsePromptBtn() {
    if (!m_promptBtn) return;

    // The ">>" marker toggles the batch panel, and that is its ONLY affordance:
    // it looks like a decorative prompt symbol. Someone whose paste just
    // teleported them into a panel they've never seen needs to know how to get
    // back, and they need to know it now -- not from a tooltip they'd have to
    // already suspect exists.
    //
    // Reuses the drop-shadow already on the button for its hover glow, so this
    // is the same visual vocabulary the user will meet again on hover, not a new
    // one invented for a one-off hint. No text, no arrow widget, no overlay to
    // position against a scrolling layout.
    auto* glow = qobject_cast<QGraphicsDropShadowEffect*>(m_promptBtn->graphicsEffect());
    if (!glow) return;

    // Kill any previous pulse -- paste twice in a row and you'd otherwise get two
    // animations fighting over one blurRadius.
    if (m_promptPulse) { m_promptPulse->stop(); m_promptPulse->deleteLater(); }

    auto* a = new QPropertyAnimation(glow, "blurRadius", m_promptBtn);
    a->setDuration(1600);
    a->setLoopCount(3);
    a->setKeyValueAt(0.0, 0.0);
    a->setKeyValueAt(0.5, 26.0);
    a->setKeyValueAt(1.0, 0.0);
    a->setEasingCurve(QEasingCurve::InOutSine);
    m_promptPulse = a;

    // Hand the glow back at 0 so the hover animation isn't left fighting a
    // leftover blur. The effect itself stays attached -- it's the hover glow's,
    // not ours to remove.
    connect(a, &QPropertyAnimation::finished, this, [this, glow]() {
        glow->setBlurRadius(0);
        m_promptPulse = nullptr;
        });
    a->start(QAbstractAnimation::DeleteWhenStopped);
}

QWidget* MainWindow::createSidebar() {
    auto* sidebar = new QWidget;
    sidebar->setFixedWidth(290);
    sidebar->setStyleSheet("background:transparent;");

    auto* sbL = new QVBoxLayout(sidebar);
    sbL->setContentsMargins(0, 0, 0, 0);
    sbL->setSpacing(12);

    // -- Quick Reference -------------------------------------------------------
    auto* refFrame = new QFrame;
    refFrame->setObjectName("refFrame");
    refFrame->setStyleSheet(QString(
        "QFrame#refFrame{background:%1;border:1px solid %2;border-radius:12px;}"
    ).arg(C_SURFACE, C_BORDER));

    auto* refVL = new QVBoxLayout(refFrame);
    refVL->setContentsMargins(0, 0, 0, 0);
    refVL->setSpacing(0);

    auto* refHead = new QLabel("QUICK REFERENCE");
    refHead->setFont(MF(8));
    refHead->setStyleSheet(QString(
        "background:%1;border-bottom:1px solid %2;"
        "border-top-left-radius:12px;border-top-right-radius:12px;"
        "padding:9px 14px;color:%3;letter-spacing:2px;"
    ).arg(C_CARD, C_BORDER, C_MUTED));

    m_refPanel = new SidebarPanel;
    connect(m_refPanel, &SidebarPanel::itemClicked,
        this, &MainWindow::onSidebarItemClicked);
    connect(m_refPanel, &SidebarPanel::itemDoubleClicked,
        this, &MainWindow::onSidebarItemDoubleClicked);

    refVL->addWidget(refHead);
    refVL->addWidget(m_refPanel, 1);
    sbL->addWidget(refFrame, 1);

    // -- Session ---------------------------------------------------------------
    m_sessionPanel = new SessionPanel;
    connect(m_sessionPanel, &SessionPanel::clearRequested, this, &MainWindow::onClear);
    sbL->addWidget(m_sessionPanel);

    return sidebar;
}

void MainWindow::ensureSettingsPage() {
    if (m_settingsPage) return;

    m_settingsPage = new QWidget;
    m_settingsPage->setStyleSheet(QString("background:%1;").arg(Theme::BG));

    auto* layout = new QVBoxLayout(m_settingsPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_settingsBackBtn = new QPushButton(">_");
    m_settingsBackBtn->setFixedSize(32, 32);
    m_settingsBackBtn->setFont(MF(13));
    m_settingsBackBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBackBtn->setToolTip("Back to terminal");

    auto* topBar = new QWidget;
    topBar->setStyleSheet("background:transparent;");
    topBar->setFixedHeight(40);
    auto* topBarL = new QHBoxLayout(topBar);
    topBarL->setContentsMargins(0, 4, 8, 4);
    topBarL->setSpacing(0);
    topBarL->addStretch(1);
    topBarL->addWidget(m_settingsBackBtn, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(topBar);

    WR_ADD(m_settingsBackBtnId, m_settingsBackBtn, WidgetRole::FontUI);
    WidgetRegistry::instance().setStylesheet(m_settingsBackBtnId, QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;"
        "border-radius:6px;padding:0px;}"
        "QPushButton:hover{border-color:%2;color:%2;}"
    ).arg(C_BORDER, Theme::ACCENT()));

    m_settingsPageWidget = new SettingsPage;
    layout->addWidget(m_settingsPageWidget, 1);

    connect(m_settingsPageWidget, &SettingsPage::copyHistoryRequested, this, [this]() {
        if (m_history.isEmpty()) return;
        QApplication::clipboard()->setText(m_history.join('\n'));
        });

    connect(m_settingsBackBtn, &QPushButton::clicked, this, [this]() {
        m_settingsPageWidget->prepareToLeave([this]() {
            m_centralStack->setCurrentWidget(m_terminalPage);
            m_settingsBackBtn->hide();
            m_settingsBtn->show();
            });
        });

    m_settingsBackBtn->hide();
    m_centralStack->addWidget(m_settingsPage);
}

void MainWindow::ensureFeaturePage() {
    if (m_featurePage) return;

    m_featurePage = new FeatureViewerPage;
    m_centralStack->addWidget(m_featurePage);

    connect(m_featurePage, &FeatureViewerPage::backClicked, this, [this]() {
        m_centralStack->setCurrentWidget(m_terminalPage);
        });

    // Clicking an example returns to the terminal and runs it — but never onto
    // a job already in flight.
    connect(m_featurePage, &FeatureViewerPage::runExample, this, [this](const QString& expr) {
        m_centralStack->setCurrentWidget(m_terminalPage);
        if (m_state != RunState::Idle) return;
        StateUtils::setRunSource(RunSource::User);
        submitExpression(expr);
        m_input->clear();
        m_input->setFocus();
        });
}