#include "MainWindow.h"
#include "OutputArea.h"
#include "SidebarPanel.h"
#include "../math/MathEngine.h"
#include "../input/InputHandler.h"
#include "CRTTextLabel.h"
#include "Animations.h"
#include <QMessageBox>
#include "../constants/Theme.h"
#include "..\input\InputRouter.h"
#include "HistoryDock.h"
#include "DraggableExpressionEdit.h"
#include <QGraphicsDropShadowEffect>
#include "FocusGlow.h"
#include "GeoModeWidget.h"
#include "settings/SettingsPage.h"
#include "../settings/Settings.h"
#include <QApplication>
#include <QClipboard>

static QString g_fontFamily;
static void initFont() {
    static bool done = false;
    if (done) return; done = true;
    for (const QString& f : { "JetBrains Mono","Consolas","Courier New" })
        if (QFontDatabase::families().contains(f)) { g_fontFamily = f; return; }
    g_fontFamily = "Courier New";
}
static QFont MF(int pt, int w = QFont::Normal) {
    initFont();
    QFont f(g_fontFamily); f.setPointSize(pt); f.setWeight((QFont::Weight)w);
    f.setStyleHint(QFont::Monospace); return f;
}

// -----------------------------------------------------------------------------
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("MATHX \xe2\x80\x94 Unlimited Calculator");
    setMinimumSize(900, 640); resize(1100, 750);
    setStyleSheet(QString("QMainWindow,QWidget{background:%1;color:%2;}").arg(C_BG, C_TEXT));

    // Session uptime — starts the moment the window is constructed
    m_sessionTimer.start();
    m_runtimeUpdateTimer = new QTimer(this);
    connect(m_runtimeUpdateTimer, &QTimer::timeout, this, &MainWindow::updateRuntimeLabel);
    m_runtimeUpdateTimer->start(1000);

    // m_focusAnchor = new FocusAnchor(this);

    setupUi();



    /*// FocusGlow inspector panel — hidden until the user clicks the FocusAnchor button
    m_focusGlow = new FocusGlow(this, centralWidget());
    // FocusAnchor acts as a toggle button to show/hide the inspector
    connect(m_focusAnchor, &FocusAnchor::clicked, this, [this]() {
        if (m_focusGlow->isVisible()) {
            m_focusGlow->hide();
            m_focusAnchor->setActive(false);
        }
        else {
            // Position panel just below the anchor button on first show
            QPoint anchorGlobal = m_focusAnchor->mapToGlobal(QPoint(0, m_focusAnchor->height() + 6));
            m_focusGlow->move(anchorGlobal);
            m_focusGlow->show();
            m_focusAnchor->setActive(true);
        }
        m_input->setFocus(); // never let inspector steal focus from input
        });

    // If the user closes the panel via its × button, deactivate the anchor too
    connect(m_focusGlow, &FocusGlow::closedByUser, this, [this]() {
        m_focusAnchor->setActive(false);
        m_input->setFocus();
        });
    */
    m_input->setFocus();
    /*m_historyDock = new HistoryDock(this);
    addDockWidget(Qt::RightDockWidgetArea, m_historyDock);
    connect(m_historyDock, &HistoryDock::expressionSelected, this, [this](const QString& expr) {
        m_input->setText(expr);
        m_input->setFocus();
        });*/
        // Create persistent worker and thread
    m_workerThread = new QThread(this);
    m_worker = new PersistentWorker();
    m_worker->moveToThread(m_workerThread);

    // Connect signals
    connect(m_workerThread, &QThread::started, m_worker, &PersistentWorker::process);
    connect(m_worker, &PersistentWorker::resultReady, this, &MainWindow::onWorkerFinish);


    connect(this, &MainWindow::stop, m_worker, &PersistentWorker::cancelAll);
    connect(m_worker, &PersistentWorker::progress, this, [this](int jobId, int percent, const QString& label) {
        m_output->showProgress(percent, label);
        });
    connect(m_output, &OutputArea::shapeProjectionRequested, this, &MainWindow::onShowGeometryMode);
    connect(m_geoModeWidget, &GeoModeWidget::backClicked, this, [this]() {
        QLayout* root = centralWidget()->layout();
        root->setContentsMargins(m_originalMargins);
        root->setSpacing(m_originalSpacing);
        m_centralStack->setCurrentIndex(0);
        });
    m_workerThread->start();
}
// -- updateRuntimeLabel ---------------------------------------------------------
// Ticks once per second via m_runtimeUpdateTimer. Uses QElapsedTimer (monotonic)
// rather than a QDateTime diff, so the displayed uptime is immune to system
// clock changes — sleep/resume, manual clock adjustment, timezone shifts, etc.
void MainWindow::updateRuntimeLabel() {
    if (!m_runtimeLbl) return;

    const qint64 totalSeconds = m_sessionTimer.elapsed() / 1000;
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;

    QString runtime;
    if (hours > 0)
        runtime += QString::number(hours) + "h ";
    if (minutes > 0 || hours > 0)
        runtime += QString::number(minutes) + "m ";
    runtime += QString::number(seconds) + "s";

    m_runtimeLbl->setText(runtime);
}

void MainWindow::onShowProjection(const QString& type, const QMap<QString, double>& params) {
    m_geoModeWidget->setShape(type, params);
    m_centralStack->setCurrentIndex(1);
}
void MainWindow::onShowGeometryMode(const QString& type, const QMap<QString, double>& params) {
    recreateGeometryMode();
    m_geoModeWidget->setShape(type, params);
    // Remove margins and spacing for full‑window black background
    QLayout* root = centralWidget()->layout();
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    m_centralStack->setCurrentIndex(1);
}
void MainWindow::recreateGeometryMode() {
    if (m_geoModeWidget) {
        m_centralStack->removeWidget(m_geoModeWidget);
        delete m_geoModeWidget;
        m_geoModeWidget = nullptr;
    }
    m_geoModeWidget = new GeoModeWidget;
    m_centralStack->addWidget(m_geoModeWidget);
    // Reconnect the back button signal
    connect(m_geoModeWidget, &GeoModeWidget::backClicked, this, [this]() {
        QLayout* root = centralWidget()->layout();
        root->setContentsMargins(m_originalMargins);
        root->setSpacing(m_originalSpacing);
        m_centralStack->setCurrentIndex(0);
        });
}
void MainWindow::onWorkerFinish(int jobId, const QString& result, const QString& type, const QString& formula) {
    // Retrieve the original expression (needed for geometry cards)
    QString expr = m_pendingJobs.take(jobId);

    if (type == "geo") {
        // Geometry card – must be created on the UI thread
        GeoCard* card = InputHandler::makeGeoCard(expr);
        if (card) {
            m_output->addGeoCard(card);
            m_copyHistBuffer.append(card->buildCopyText().append("\n\n"));
        }



        else {
            // Possibly missing parameters – start prompt or show error
            QStringList missing = InputHandler::missingRequiredParams(expr);
            if (!missing.isEmpty()) {
                // Start interactive prompt for missing parameters
                QString baseCmd = expr.split('(').first().split(' ').first();
                QString canonical = InputHandler::resolveAlias(baseCmd);
                QStringList allParams = InputHandler::getRequiredParams(canonical);
                QMap<QString, QString> prefill;
                // Extract already provided parameters from expr
                QRegularExpression re(R"(([a-z]+)\s*=\s*([^\s]+))");
                auto it = re.globalMatch(expr.toLower());
                while (it.hasNext()) {
                    auto m = it.next();
                    prefill[m.captured(1)] = m.captured(2);
                }
                int providedCount = 0;
                for (const QString& p : allParams) {
                    if (prefill.contains(p)) providedCount++;
                    else break;
                }

                ShapePrompt prompt;
                prompt.command = canonical;
                prompt.params = allParams;
                prompt.currentIndex = providedCount;
                prompt.collected = prefill;
                setRunState(RunState::HandlingInput);
                m_promptCtrl->start(prompt);
                m_output->addPromptRequest(m_promptCtrl->currentParam());
            }
            else {
                m_output->addResultLine("Invalid shape expression", "err");
                m_output->addSeparator();
            }
        }
    }
    // Fix #9: was `else { addResultLine(...) }` which also fired for "geo" results.
    //         Split into separate branches so geo, big, and everything else are exclusive.
    if (type == "big") {
        const int fullLen = result.length();
        const int limit = 500;
        if (fullLen > limit) {
            QString display = result.left(limit);
            display.append(QString("\n.... %1 more characters (%2 total)")
                .arg(fullLen - limit).arg(fullLen));
            m_output->addResultLine(display, "big", result);
            m_copyHistBuffer.append(result);
        }
        else {
            m_output->addResultLine(result, "big");
            m_copyHistBuffer.append(QString(result).append("\n\n"));
        }
    }
    else if (type != "geo") {


        // Normal result (arithmetic, conversion, algebra, trig, big number, error)
        m_output->addResultLine(result, type, result, formula, expr);
        m_copyHistBuffer.append((QString(result).append("\n\n")));
    }

    if (m_output->progressState() == true) m_output->hideProgress();
    // Update session display
    m_lastResult = result;
    if (type != "geo") m_lastResultLbl->setText(result.length() > 23 ? result.first(20) + "..." : result); else m_lastResultLbl->setText(QString("Geometry Card"));
    m_output->addSeparator();
    // if(type != result) m_historyDock->addEntry(expr, result);
     // Go back to idle state (allow new input)
    if (!m_promptCtrl->isActive()) {
        setRunState(RunState::Idle);
    }

}

void MainWindow::onCopyHistory()
{
    QString copyText = "";
    for (QString entry : m_copyHistBuffer) {
        
        copyText.append(entry);
    }
    QApplication::clipboard()->setText(copyText);
}



// -- eventFilter ---------------------------------------------------------------
bool MainWindow::eventFilter(QObject* obj, QEvent* ev) {
    if (obj != m_input || ev->type() != QEvent::KeyPress)
        return QMainWindow::eventFilter(obj, ev);

    auto* ke = static_cast<QKeyEvent*>(ev);

    InputAction action = InputRouter::translate(
        ke,
        m_promptCtrl->isActive(),
        m_history.isEmpty()
    );

    switch (action) {
    case InputAction::CancelPrompt:
        m_promptCtrl->cancel();
        return true;
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

void MainWindow::closeEvent(QCloseEvent* closeEvent)
{
    if (m_worker) {
        m_worker->stop();
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }

}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    /*if (m_termTitle && m_termBar)
    {
        const int x =
            (m_termBar->width() - m_termTitle->width()) / 2;

        const int y =
            (m_termBar->height() - m_termTitle->height()) / 2;

        m_termTitle->move(x, y);
    }*/
}

// -- setRunState ---------------------------------------------------------------
void MainWindow::setRunState(RunState state) {
    m_state = state;
    switch (state) {
    case RunState::Idle:
        m_stopBtn->hide();
        m_runBtn->show();
        m_input->setEnabled(true);
        if (!m_promptCtrl->isActive())
            m_input->setPlaceholderText("Enter expression, equation, or shape...");
        m_input->setFocus();
        // Apply any staged settings that were waiting for idle
        Settings::instance().applyPending(true);
        break;
    case RunState::HandlingInput:
        m_runBtn->hide();
        m_stopBtn->show();
        m_input->setEnabled(true);
        break;

    case RunState::Running:
        m_stopBtn->show();
        m_runBtn->hide();
        m_input->setEnabled(false);
        break;
    case RunState::Stopping:
        emit stop();
        break;
    }
}



// -- submitExpression ----------------------------------------------------------
void MainWindow::submitExpression(const QString& expr) {
    m_output->addInputLine(expr);
    m_lastExprLbl->setText(expr);
    m_calcCount++;
    m_countLbl->setText(QString::number(m_calcCount));
    m_histNav->reset();
    setRunState(RunState::Running);

    run(expr);
}

// -- onRun ---------------------------------------------------------------------
void MainWindow::onRun() {
    // Animations::flash(m_runBtn);
  //   Animations::flash(m_stopBtn);
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    m_input->clear();

    if (m_promptCtrl->isActive()) {
        setRunState(RunState::HandlingInput);
        handlePromptInput(text);
        return;
    }
    if (!tryStartPrompt(text)) {
        setRunState(RunState::HandlingInput);
        submitExpression(text);
    }

}


/*m_history.append(expr);
    m_lastExprLbl->setText(expr);

    // Factorial — async path
    // Match both fact(n) and n! notations
    static QRegularExpression factRe(
        R"(^\s*(?:fact\s*\(\s*(-?\d+)\s*\)|(-?\d+)\s*!)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto match = factRe.match(expr);
    if (match.hasMatch()) {
        // Group 1 = fact(n) form, group 2 = n! form
        QString nStr = match.captured(1).isEmpty() ? match.captured(2) : match.captured(1);
        BigInt n(nStr.toStdString());
        if (n < 0) {
            m_output->addResultLine("Negative factorials are not allowed!", "err");
            m_output->addSeparator();
            setRunState(RunState::Idle);
            return;
        }
        computeFactorialAsync(n);
        return;
    }

    // Synchronous path
    try {
        CalcResult res = MathEngine::evaluate(expr);
        if (res.type == "fact") {
            // Postfix n! routed back from MathEngine via __BIGFACT__ exception
            bool nOk;
            long long n = res.result.toLongLong(&nOk);
            if (!nOk || n < 0) {
                m_output->addResultLine("Factorial of negative number", "err");
                m_output->addSeparator();
                setRunState(RunState::Idle);
            }
            else {
                computeFactorialAsync(BigInt(n));
            }
        }
        else if (res.type == "geo") {
            QThread* calcThread = new QThread;
            GeoCard* card = InputHandler::makeGeoCard(expr);
            if (card) {
                m_output->addGeoCard(card);
            }
            else {
                QStringList missing = InputHandler::missingRequiredParams(expr);
                if (!missing.isEmpty()) {
                    QString baseCmd = expr.split('(').first().split(' ').first();
                    QString canonical = InputHandler::resolveAlias(baseCmd);
                    QStringList allParams = InputHandler::getRequiredParams(canonical);
                    QMap<QString, QString> prefill;
                    QRegularExpression re(R"(([a-z]+)\s*=\s*([^\s]+))");
                    auto it = re.globalMatch(expr.toLower());
                    while (it.hasNext()) {
                        auto m = it.next();
                        prefill[m.captured(1)] = m.captured(2);
                    }
                    int providedCount = 0;
                    for (const QString& p : allParams) {
                        if (prefill.contains(p)) providedCount++;
                        else break;
                    }
                    ShapePrompt prompt;
                    prompt.command = canonical;
                    prompt.params = allParams;
                    prompt.currentIndex = providedCount;
                    prompt.collected = prefill;
                    setRunState(RunState::HandlingInput);
                    m_promptCtrl->start(prompt);
                    m_output->addPromptRequest(m_promptCtrl->currentParam());

                }
                else {
                    m_output->addResultLine("Invalid shape expression", "err");
                    m_output->addSeparator();
                    setRunState(RunState::Idle);
                }
            }
        }
        else {
            m_output->addResultLine(res.result, res.type);
            m_lastResult = res.result;
            m_lastResultLbl->setText(
                m_lastResult.length() > 23
                ? m_lastResult.first(20) + "..."
                : m_lastResult);
        }
    }
    catch (std::exception& ex) {
        m_output->addResultLine(QString("\xe2\x9a\xa0 ") + ex.what(), "err");
        m_output->addSeparator();
        setRunState(RunState::Idle);
        return;
    }
    m_output->addSeparator();

    */
    // -- run -----------------------------------------------------------------------
void MainWindow::run(const QString& expr) {

    m_copyHistBuffer.append(QString(expr).prepend(">> ").append("\n"));
    m_history.append(QString(expr));
    m_lastExprLbl->setText(expr);
    setRunState(RunState::Running);
    int id = ++m_nextJobId;
    m_pendingJobs[id] = expr;
    m_worker->submitJob(id, expr);
}

// -- onCalcFinish --------------------------------------------------------------
void MainWindow::onCalcFinish(RunState state, QString result) {
    setRunState(state);
    m_lastResult = result;
    m_lastResultLbl->setText(
        result.length() > 23 ? result.first(20) + "..." : result);
}

//-- onStop ---------------------------------------------------------------
void MainWindow::onStop() {
    // Cancel any active shape prompt
    if (m_promptCtrl && m_promptCtrl->isActive()) {
        m_promptCtrl->cancel();
    }
    if (m_worker) {
        m_worker->cancelAll();
    }
    setRunState(RunState::Stopping);
}

// -- onTruncateClicked ---------------------------------------------------------
void MainWindow::onTruncateClicked() {
    // Toggle truncation behaviour — placeholder for future implementation
}

void MainWindow::onAsyncResult(int jobId, const QString& result, const QString& type) {
    if (type == "geo") {
        QString expr = m_pendingJobs.take(jobId);
        GeoCard* card = InputHandler::makeGeoCard(expr);
        if (card) {
            m_output->addGeoCard(card);
        }
        else {
            m_output->addResultLine("Invalid shape expression", "err");
        }
    }
    else {
        m_output->addResultLine(result, type);
    }
    m_lastResult = result;
    m_lastResultLbl->setText(result.length() > 23 ? result.first(20) + "..." : result);
    m_output->addSeparator();
    setRunState(RunState::Idle);
}

// -- PromptController slots ----------------------------------------------------
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
    m_output->addResultLine("  Input operation cancelled by user", "ok");
    m_output->addSeparator();
    setRunState(RunState::Idle);
}

// -- handlePromptInput ---------------------------------------------------------
void MainWindow::handlePromptInput(const QString& value) {
    QString param = m_promptCtrl->currentParam();
    double numericValue = 0.0;
    bool ok = false;

    numericValue = value.toDouble(&ok);

    if (!ok && !value.contains('=')) {
        bool evalOk = false;
        double v = MathEngine::evalSimple(value, evalOk);
        if (evalOk && std::isfinite(v)) {
            numericValue = v; ok = true;
            m_output->addResultLine(
                QString("  (evaluated '%1' \xe2\x86\x92 %2)")
                .arg(value, QString::number(v, 'g', 10)), "ok");
        }
    }

    if (!ok && value.contains('=')) {
        double solved = 0.0;
        if (MathEngine::solveEquation(value, solved)) {
            QMessageBox mb;
            mb.setWindowTitle("Equation Solved");
            mb.setText(QString("'%1' solves to %2 = %3").arg(value, param).arg(solved));
            mb.setInformativeText("Use this value?");
            mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            mb.setDefaultButton(QMessageBox::Yes);
            if (mb.exec() == QMessageBox::Yes) {
                numericValue = solved; ok = true;
            }
            else {
                m_output->addResultLine(
                    "Equation solution rejected. Please enter a number.", "err");
                m_output->addPromptRequest(param);
                m_input->setFocus();
                return;
            }
        }
    }
    if (!ok && value.contains('=')) {
        double solved = 0.0;
        if (MathEngine::solveEquation(value, solved)) {
            QMessageBox mb;
            mb.setWindowTitle("Equation Solved");
            mb.setText(QString("'%1' solves to %2 = %3").arg(value, param).arg(solved));
            mb.setInformativeText("Use this value?");
            mb.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
            mb.setDefaultButton(QMessageBox::Yes);
            if (mb.exec() == QMessageBox::Yes) {
                numericValue = solved; ok = true;
                m_promptVars[param] = numericValue;
                m_promptCtrl->submit(value, numericValue);
                return;
            }
            else {
                m_output->addResultLine(
                    "Equation solution rejected. Please enter a number.", "err");
                m_output->addPromptRequest(param);
                m_input->setFocus();
                return;
            }
        }
    }
    if (!ok) {
        m_output->addResultLine(
            QString("\xe2\x9a\xa0 Invalid input for '%1'").arg(param), "err");
        m_output->addPromptRequest(param);
        m_input->setFocus();
        return;
    }

    m_promptCtrl->submit(value, numericValue);
}

// -- tryStartPrompt ------------------------------------------------------------
bool MainWindow::tryStartPrompt(const QString& expr) {
    ShapePrompt p = InputHandler::detectPrompt(expr);
    if (!p.isActive()) return false;
    setRunState(RunState::HandlingInput);
    m_promptVars.clear();
    m_promptCtrl->start(p);
    m_output->addPromptRequest(m_promptCtrl->currentParam());
    return true;
}

// -- Mode / sidebar ------------------------------------------------------------
void MainWindow::onModeChanged(const QString& mode) {
    m_currentMode = mode;
    m_refPanel->renderMode(mode);
}

void MainWindow::onSidebarItemClicked(const QString& expr) {
    if (getRunState() != RunState::Idle) return;


    m_input->setText(expr);
    m_input->setFocus();
}

void MainWindow::onSidebarItemDoubleClicked(const QString& expr) {
    if (getRunState() != RunState::Idle) return;


    submitExpression(expr);
    m_input->clear();
    m_input->setFocus();
}

// -- onClear -------------------------------------------------------------------
void MainWindow::onClear() {
    m_history.clear();
    m_histNav->reset();
    m_promptCtrl->reset();
    m_calcCount = 0;
    m_lastResult.clear();
    m_lastExprLbl->setText("");
    m_lastResultLbl->setText("\xe2\x80\x94");
    m_countLbl->setText("0");
    m_output->clearAll();
    m_input->clear();
    setRunState(RunState::Idle);
}

RunState MainWindow::getRunState() { return m_state; }

MainWindow::~MainWindow() {
    if (m_worker) {
        m_worker->stop();   // tell worker to exit
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(5000); // wait up to 5 seconds
        delete m_worker;
        delete m_workerThread;
    }
}

// -- setupUi -------------------------------------------------------------------
/*void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    central->setStyleSheet(QString("background:%1;").arg(C_BG));
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(24, 20, 24, 24); root->setSpacing(16);


    setupHeader();
    root->addWidget(m_header);

    auto* mainWrap = new QWidget;
    mainWrap->setStyleSheet("background:transparent;");
    auto* mainL = new QHBoxLayout(mainWrap);
    mainL->setContentsMargins(0, 0, 0, 0); mainL->setSpacing(16);



    setupTerminal();
    // Create stacked widget
    m_centralStack = new QStackedWidget;
    m_centralStack->addWidget(m_terminal);   // page 0
    m_geoModeWidget = new GeoModeWidget;
    m_centralStack->addWidget(m_geoModeWidget); // page 1
    mainL->addWidget(m_centralStack, 1);



    auto* sidebar = new QWidget;
    sidebar->setFixedWidth(290);
    sidebar->setStyleSheet("background:transparent;");
    auto* sbL = new QVBoxLayout(sidebar);
    sbL->setContentsMargins(0, 0, 0, 0); sbL->setSpacing(12);


    auto* refFrame = new QFrame; refFrame->setObjectName("refFrame");
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
    connect(m_refPanel, &SidebarPanel::itemClicked, this, &MainWindow::onSidebarItemClicked);
    connect(m_refPanel, &SidebarPanel::itemDoubleClicked, this, &MainWindow::onSidebarItemDoubleClicked);
    refVL->addWidget(refHead);
    refVL->addWidget(m_refPanel, 1);
    sbL->addWidget(refFrame, 1);


    auto* sessFrame = new QFrame; sessFrame->setObjectName("sessFrame");
    sessFrame->setStyleSheet(QString(
        "QFrame#sessFrame{background:%1;border:1px solid %2;border-radius:12px;}"
    ).arg(C_SURFACE, C_BORDER));


    auto* sessVL = new QVBoxLayout(sessFrame);
    sessVL->setContentsMargins(0, 0, 0, 0); sessVL->setSpacing(0);


    auto* sessHead = new QLabel("SESSION"); sessHead->setFont(MF(8));
    sessHead->setStyleSheet(QString(
        "background:%1;border-bottom:1px solid %2;"
        "border-top-left-radius:12px;border-top-right-radius:12px;"
        "padding:9px 14px;color:%3;letter-spacing:2px;"
    ).arg(C_CARD, C_BORDER, C_MUTED));


    auto* sessBody = new QWidget; sessBody->setStyleSheet("background:transparent;");
    auto* sessBodyL = new QVBoxLayout(sessBody);
    sessBodyL->setContentsMargins(14, 10, 14, 12); sessBodyL->setSpacing(5);


    auto makeRow = [&](const QString& t, QLabel*& valLbl, const QString& valColor) {
        auto* row = new QHBoxLayout; row->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(t); lbl->setFont(MF(9));
        lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(C_MUTED));
        valLbl = new QLabel("\xe2\x80\x94"); valLbl->setFont(MF(9));
        valLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(valColor));
        row->addWidget(lbl); row->addSpacing(6); row->addWidget(valLbl); row->addStretch();
        sessBodyL->addLayout(row);
        };

    makeRow("Calculations:", m_countLbl, C_TEXT); m_countLbl->setText("0");
    makeRow("Last result:", m_lastResultLbl, C_ACCENT);
    makeRow("Last Expression Ran:", m_lastExprLbl, C_ACCENT);
    auto* clearBtn = new QPushButton("Clear History"); clearBtn->setFont(MF(9));
    clearBtn->setFixedHeight(26);
    clearBtn->setStyleSheet(QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;padding:2px 12px;border-radius:6px;}"
        "QPushButton:hover{border-color:%3;color:%3;}"
    ).arg(C_BORDER, C_MUTED, C_ERR));
    connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClear);
    sessBodyL->addSpacing(2); sessBodyL->addWidget(clearBtn);
    sessVL->addWidget(sessHead); sessVL->addWidget(sessBody);
    sbL->addWidget(sessFrame);

    mainL->addWidget(sidebar);

    root->addWidget(mainWrap, 1);
}
*/
void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    central->setStyleSheet(QString("background:%1;").arg(C_BG));
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(24, 20, 24, 24);
    root->setSpacing(16);
    m_originalMargins = root->contentsMargins();
    m_originalSpacing = root->spacing();

    // No header added to root layout – it will go inside page 0

    // Create stacked widget
    m_centralStack = new QStackedWidget;
    m_centralStack->setStyleSheet("border: none;");
    // -- Page 0: Terminal mode (with header and sidebar) -----------------
    QWidget* terminalPage = new QWidget;
    QVBoxLayout* page0Layout = new QVBoxLayout(terminalPage);
    page0Layout->setContentsMargins(0, 0, 0, 0);
    page0Layout->setSpacing(16);

    // Add header at the top of terminal page
    setupHeader();          // creates m_header
    page0Layout->addWidget(m_header);

    // Horizontal container for terminal + sidebar
    QWidget* contentRow = new QWidget;
    QHBoxLayout* rowLayout = new QHBoxLayout(contentRow);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(16);

    setupTerminal();       // creates m_terminal
    rowLayout->addWidget(m_terminal, 1);

    // Sidebar (Quick Reference + Session)
    QWidget* sidebar = createSidebar();
    rowLayout->addWidget(sidebar);

    page0Layout->addWidget(contentRow, 1);   // give all extra space to content

    m_centralStack->addWidget(terminalPage);

    // -- Page 1: Geometry mode (full‑window, no header, no sidebar) ------
    m_geoModeWidget = new GeoModeWidget;
    m_centralStack->addWidget(m_geoModeWidget);

    // -- Page 2: Settings -------------------------------------------------
    m_settingsPage = new QWidget;
    m_settingsPage->setStyleSheet(QString("background:%1;").arg(Theme::BG));
    auto* settingsLayout = new QVBoxLayout(m_settingsPage);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(0);

    // >_ back button — top right, always visible on the settings page
    m_settingsBackBtn = new QPushButton(">_");
    m_settingsBackBtn->setFixedSize(32, 32);
    m_settingsBackBtn->setFont(MF(13));
    m_settingsBackBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBackBtn->setToolTip("Back to terminal");
    m_settingsBackBtn->setStyleSheet(QString(
        "QPushButton { background:none; border:1px solid %1; color:%2;"
        "border-radius:6px; padding:0px; }"
        "QPushButton:hover { border-color:%2; color:%2; }"
    ).arg(C_BORDER, C_ACCENT));

    // Back button sits in a thin top bar so it doesn't overlap the settings chrome
    auto* settingsTopBar = new QWidget;
    settingsTopBar->setStyleSheet("background:transparent;");
    settingsTopBar->setFixedHeight(40);
    auto* topBarL = new QHBoxLayout(settingsTopBar);
    topBarL->setContentsMargins(0, 4, 8, 4);
    topBarL->setSpacing(0);
    topBarL->addStretch(1);
    topBarL->addWidget(m_settingsBackBtn, 0, Qt::AlignRight | Qt::AlignVCenter);
    settingsLayout->addWidget(settingsTopBar);

    // The actual settings page UI
    m_settingsPageWidget = new SettingsPage;
    settingsLayout->addWidget(m_settingsPageWidget, 1);

    connect(m_settingsPageWidget, &SettingsPage::copyHistoryRequested, this, [this]() {
        if (m_history.isEmpty()) return;
        QApplication::clipboard()->setText(m_history.join('\n'));
        });

    // Back button wired through prepareToLeave so pending changes apply first
    connect(m_settingsBackBtn, &QPushButton::clicked, this, [this]() {
        m_settingsPageWidget->prepareToLeave([this]() {
            m_centralStack->setCurrentIndex(0);
            m_settingsBackBtn->hide();
            m_settingsBtn->show();
            });
        });

    m_settingsBackBtn->hide(); // hidden until settings is opened
    m_centralStack->addWidget(m_settingsPage);

    root->addWidget(m_centralStack, 1);
}
// -- setupHeader ---------------------------------------------------------------
/*void MainWindow::setupHeader() {
    m_header = new QWidget; m_header->setStyleSheet("background:transparent;");
    auto* hl = new QHBoxLayout(m_header); hl->setContentsMargins(0, 0, 0, 0);
    initFont();
    auto* logoArea = new QWidget; logoArea->setStyleSheet("background:transparent;");
    auto* logoL = new QVBoxLayout(logoArea); logoL->setContentsMargins(0, 0, 0, 0); logoL->setSpacing(1);
    auto* logo = new QLabel;


    CRTTextLabel* logoo = new CRTTextLabel("MATHX", this);
    logoo->setSubtitle("UNLIMITED CALCULATOR", 8);
    logoo->setGlowColor(QColor(0, 255, 65));
    logoo->setGlowLayers(3);
    logoo->setScanlineOpacity(40);
    logoo->setFlickerInterval(80);
    logoo->setFlickerEnabled(true);
    logoo->loadFontFromFile(":/fonts/Orbitron-VariableFont_wght.ttf", 20);

    // Or let user pick any font at runtime:
    // logo->openFontPicker();  // or wire to a button
 //   QFont lf("Syne"); lf.setPointSize(20); lf.setWeight(QFont::ExtraBold);
 //   if (!QFontDatabase::families().contains("Syne")) lf.setFamily(g_fontFamily);

  //  logo->setFont(lf); logo->setTextFormat(Qt::RichText);
  //  logo->setText(QString("<span style='color:%1;'>MATH</span><span style='color:%2;'>X</span>")
   //     .arg(C_ACCENT, C_TEXT));

  //  auto* glow = new QGraphicsDropShadowEffect;
   // glow->setBlurRadius(18); glow->setColor(QColor("#00e87a")); glow->setOffset(0, 0);
  //  logo->setGraphicsEffect(glow); logo->setStyleSheet("background:transparent;");
  //  auto* tagline = new QLabel("UNLIMITED CALCULATOR"); tagline->setFont(MF(7));
 //   tagline->setStyleSheet(QString("color:%1;background:transparent;letter-spacing:2px;").arg(C_MUTED));
 //   logoL->addWidget(logo); logoL->addWidget(tagline);
   hl->addStretch();

    m_modesBar = new QWidget; m_modesBar->setStyleSheet("background:transparent;");
    auto* ml = new QHBoxLayout(m_modesBar); ml->setContentsMargins(0, 0, 0, 0); ml->setSpacing(6);
    struct ME { QString label, key; };
    QList<ME> modes = { {"All","all"},{"Arithmetic","arith"},{"Algebra","algebra"},
                       {"Trig","trig"},{"Geometry","geo"},{"Convert","conv"} };
    QString inCSS = QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;"
        "padding:2px 12px;border-radius:13px;letter-spacing:1px;}"
        "QPushButton:hover{border-color:%3;color:%3;}"
    ).arg(C_BORDER, C_MUTED, C_ACCENT_DIM);
    QString acCSS = QString(
        "QPushButton{background:%1;border:1px solid %1;color:#000;"
        "padding:2px 12px;border-radius:13px;letter-spacing:1px;font-weight:bold;}"
    ).arg(C_ACCENT);
    for (int i = 0; i < modes.size(); ++i) {
        auto* btn = new QPushButton(modes[i].label);
        btn->setProperty("modeKey", modes[i].key);
        btn->setFont(MF(8)); btn->setFixedHeight(26);
        btn->setStyleSheet(i == 0 ? acCSS : inCSS);
        if (i == 0) m_activeMode = btn;
        connect(btn, &QPushButton::clicked, this, [this, btn, inCSS, acCSS]() mutable {
            if (m_activeMode && m_activeMode != btn) m_activeMode->setStyleSheet(inCSS);
            btn->setStyleSheet(acCSS); m_activeMode = btn;
            onModeChanged(btn->property("modeKey").toString());
            });
        ml->addWidget(btn);
    }
    hl->addWidget(m_modesBar);
}*/


void MainWindow::setupHeader() {
    m_header = new QWidget;
    m_header->setStyleSheet("background:transparent;");

    auto* hl = new QHBoxLayout(m_header);
    hl->setContentsMargins(8, 0, 0, 0);  // small left padding
    hl->setSpacing(0);

    initFont();

    m_header->setAttribute(Qt::WA_TranslucentBackground);
    m_header->setAutoFillBackground(false);
    // -- Logo --------------------------------------------------------------
    CRTTextLabel* logoo = new CRTTextLabel("MATHX", this);
    logoo->setSubtitle("UNLIMITED CALCULATOR", 9);
    logoo->setGlowColor(QColor(0, 255, 65));
    logoo->setGlowLayers(3);
    logoo->setScanlineOpacity(80);
    logoo->setFlickerInterval(20);
    logoo->setFlickerEnabled(true);
    logoo->loadFontFromFile(":/fonts/Orbitron-VariableFont_wght.ttf", 20);
    logoo->setFixedSize(200, 48);        // prevent it eating horizontal space

    hl->addWidget(logoo, 0, Qt::AlignLeft | Qt::AlignVCenter);
    hl->addStretch(1);                   // spacer pushes tabs right

    // -- Mode tabs ---------------------------------------------------------
    m_modesBar = new QWidget;
    m_modesBar->setStyleSheet("background:transparent;");
    auto* ml = new QHBoxLayout(m_modesBar);
    ml->setContentsMargins(0, 0, 0, 0);
    ml->setSpacing(6);

    struct ME { QString label, key; };
    QList<ME> modes = {
        {"All","all"}, {"Arithmetic","arithmetic"}, {"Algebra","algebra"},
        {"Trigonometry","trigonometry"}, {"Geometry","geometry"}, {"Convert","conversion"}
    };

    QString inCSS = QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;"
        "padding:2px 12px;border-radius:13px;letter-spacing:1px;}"
        "QPushButton:hover{border-color:%3;color:%3;}"
    ).arg(C_BORDER, C_MUTED, C_ACCENT_DIM);

    QString acCSS = QString(
        "QPushButton{background:%1;border:1px solid %1;color:#000;"
        "padding:2px 12px;border-radius:13px;letter-spacing:1px;font-weight:bold;}"
    ).arg(C_ACCENT);

    for (int i = 0; i < modes.size(); ++i) {
        auto* btn = new QPushButton(modes[i].label);
        btn->setProperty("modeKey", modes[i].key);
        btn->setFont(MF(8));
        btn->setFixedHeight(26);
        btn->setStyleSheet(i == 0 ? acCSS : inCSS);
        if (i == 0) m_activeMode = btn;
        connect(btn, &QPushButton::clicked, this, [this, btn, inCSS, acCSS]() mutable {
            if (m_activeMode && m_activeMode != btn) m_activeMode->setStyleSheet(inCSS);
            btn->setStyleSheet(acCSS);
            m_activeMode = btn;
            onModeChanged(btn->property("modeKey").toString());
            });
        ml->addWidget(btn);
    }
    hl->addWidget(m_focusAnchor);

    hl->addWidget(m_modesBar, 0, Qt::AlignRight | Qt::AlignVCenter);

    // -- Settings button (far right) ---------------------------------------
    // Shows ⚙ when on terminal/geo — click goes to settings (page 2)
    // Shows >_ when on settings — click returns to terminal (page 0)
    m_settingsBtn = new QPushButton("⚙");
    m_settingsBtn->setFixedSize(32, 32);
    m_settingsBtn->setFont(MF(13));
    m_settingsBtn->setCursor(Qt::PointingHandCursor);
    m_settingsBtn->setToolTip("Settings");
    m_settingsBtn->setStyleSheet(QString(
        "QPushButton { background: none; border: 1px solid %1; color: %2;"
        "border-radius: 6px; padding: 0px; }"
        "QPushButton:hover { border-color: %3; color: %3; }"
    ).arg(C_BORDER, C_MUTED, C_ACCENT));

    connect(m_settingsBtn, &QPushButton::clicked, this, [this]() {
        if (m_centralStack->currentIndex() == 0) {
            m_centralStack->setCurrentIndex(2);
            m_settingsBtn->hide();
            m_settingsBackBtn->show();
        }
        });

    hl->addSpacing(8);
    hl->addWidget(m_settingsBtn, 0, Qt::AlignRight | Qt::AlignVCenter);
    hl->addSpacing(8);
    /*void MainWindow::setupHeader() {
        m_header = new QWidget; m_header->setStyleSheet("background:transparent;");
        auto* hl = new QHBoxLayout(m_header);
        hl->setContentsMargins(0, 0, 0, 0);

        // Logo
        initFont();
        auto* logoArea = new QWidget; logoArea->setStyleSheet("background:transparent;");
        auto* logoL = new QVBoxLayout(logoArea);
        logoL->setContentsMargins(0, 0, 0, 0); logoL->setSpacing(1);

        auto* logo = new QLabel;
        // Syne ExtraBold for the MATHX logo — matches the original HTML font-weight:800
        QFont lf("Syne"); lf.setPointSize(20); lf.setWeight(QFont::ExtraBold);
        if (!QFontDatabase::families().contains("Syne"))
        {
            lf.setFamily(g_fontFamily);
        }  // fallback if Syne not installed
        logo->setFont(lf); logo->setTextFormat(Qt::RichText);
        logo->setText(QString("<span style='color:%1;'>MATH</span><span style='color:%2;'>X</span>")
            .arg(C_ACCENT, C_TEXT));
        // Glow effect on the MATHX logo
        auto* glow = new QGraphicsDropShadowEffect;
        glow->setBlurRadius(18);
        glow->setColor(QColor("#00e87a"));
        glow->setOffset(0, 0);
        logo->setGraphicsEffect(glow);
        logo->setStyleSheet("background:transparent;");

        auto* tagline = new QLabel("UNLIMITED CALCULATOR"); tagline->setFont(MF(7));
        tagline->setStyleSheet(QString("color:%1;background:transparent;letter-spacing:2px;").arg(C_MUTED));

        logoL->addWidget(logo); logoL->addWidget(tagline);
        hl->addWidget(logoArea); hl->addStretch();

        // Mode buttons
        m_modesBar = new QWidget; m_modesBar->setStyleSheet("background:transparent;");
        auto* ml = new QHBoxLayout(m_modesBar);
        ml->setContentsMargins(0, 0, 0, 0); ml->setSpacing(6);

        struct ME { QString label, key; };
        QList<ME> modes = { {"All","all"},{"Arithmetic","arith"},{"Algebra","algebra"},
                           {"Trig","trig"},{"Geometry","geo"},{"Convert","conv"} };

        QString inactiveCSS = QString(
            "QPushButton{background:none;border:1px solid %1;color:%2;"
            "padding:2px 12px;border-radius:13px;letter-spacing:1px;}"
            "QPushButton:hover{border-color:%3;color:%3;}"
        ).arg(C_BORDER, C_MUTED, C_ACCENT_DIM);
        QString activeCSS = QString(
            "QPushButton{background:%1;border:1px solid %1;color:#000;"
            "padding:2px 12px;border-radius:13px;letter-spacing:1px;font-weight:bold;}"
        ).arg(C_ACCENT);

        for (int i = 0; i < modes.size(); ++i) {
            auto* btn = new QPushButton(modes[i].label);
            btn->setProperty("modeKey", modes[i].key);
            btn->setFont(MF(8)); btn->setFixedHeight(26);
            btn->setStyleSheet(i == 0 ? activeCSS : inactiveCSS);
            if (i == 0) m_activeMode = btn;

            connect(btn, &QPushButton::clicked, this, [this, btn, inactiveCSS, activeCSS]() mutable {
                if (m_activeMode && m_activeMode != btn) m_activeMode->setStyleSheet(inactiveCSS);
                btn->setStyleSheet(activeCSS);
                m_activeMode = btn;
                onModeChanged(btn->property("modeKey").toString());
                });
            ml->addWidget(btn);
        }
        hl->addWidget(m_modesBar);
    }*/
}
    // -- setupTerminal -------------------------------------------------------------
    void MainWindow::setupTerminal() {
        m_terminal = new QFrame; m_terminal->setObjectName("terminal");
        m_terminal->setStyleSheet(QString(
            "QFrame#terminal{background:%1;border:1px solid %2;border-radius:12px;}"
        ).arg(C_SURFACE, C_BORDER));
        auto* tl = new QVBoxLayout(m_terminal); tl->setContentsMargins(0, 0, 0, 0); tl->setSpacing(0);

        m_termBar = new QWidget; m_termBar->setObjectName("m_termBar"); m_termBar->setFixedHeight(36);
        m_termBar->setStyleSheet(QString(
            "QWidget#m_termBar{background:%1;border-bottom:1px solid %2;"
            "border-top-left-radius:12px;border-top-right-radius:12px;}"
        ).arg(C_CARD, C_BORDER));
        auto* tbL = new QHBoxLayout(m_termBar);
        tbL->setContentsMargins(14, 2, 16, 2); tbL->setSpacing(7);
        for (const QString& col : { "#ff5f57","#febc2e","#28c840" }) {
            auto* d = new QLabel; d->setFixedSize(12, 12);
            d->setStyleSheet(QString("background:%1;border-radius:6px;").arg(col));
            tbL->addWidget(d);
        }
       
        m_copyHistBtn = new QPushButton("Copy History");
        m_copyHistBtn->setFont(MF(10));
        m_copyHistBtn->setStyleSheet(QString(
            "QPushButton{background:none;border:1px solid %1;color:%1;"
            "padding:1px 8px;border-radius:4px;}"
            "QPushButton:hover{border-color:%2;color:%2;}"
        ).arg(Theme::MUTED, Theme::ACCENT));
        connect(m_copyHistBtn, &QPushButton::clicked, this, [this]() {
            onCopyHistory();
            m_copyHistBtn->setStyleSheet(QString(
                "QPushButton{background:%2;border:0px solid %1;color:%3;"
                "padding:1px 8px;border-radius:4px;}"
                "QPushButton:hover{border-color:%2;color:%1;}"
            ).arg(Theme::MUTED, Theme::ACCENT, QString("#000000")));
            m_copyHistBtn->setText("Copied!");
            QTimer::singleShot(1500, this, [this]() {
                m_copyHistBtn->setText("Copy History");
                m_copyHistBtn->setStyleSheet(QString(
                    "QPushButton{background:none;border:1px solid %1;color:%1;"
                    "padding:1px 8px;border-radius:4px;}"
                    "QPushButton:hover{border-color:%2;color:%2;}"
                ).arg(Theme::MUTED, Theme::ACCENT));
                });
            });
        m_copyHistBtn->setFixedSize(112, 19);

        m_termTitle = new QLabel("mathx v1.0 \xe2\x80\x94 session", m_termBar);
        m_termTitle->raise();
        m_termTitle->setFont(MF(9));
        m_termTitle->setStyleSheet(QString("color:%1;background:transparent;letter-spacing:1px;").arg(C_MUTED));

        tbL->addStretch();
        tbL->addWidget(m_termTitle, 0, Qt::AlignHCenter);
        tbL->addStretch();
        tbL->addWidget(m_copyHistBtn, 0, Qt::AlignRight);

        qDebug() << "bar width =" << m_termBar->width();
        qDebug() << "title x =" << m_termTitle->geometry().center().x();
       
        m_output = new OutputArea;

        m_inputRow = new QFrame; m_inputRow->setObjectName("inputRow"); m_inputRow->setFixedHeight(52);
        auto* inputRow = m_inputRow;
        inputRow->setStyleSheet(QString(
            "QFrame#inputRow{background:%1;border-top:1px solid %2;"
            "border-bottom-left-radius:12px;border-bottom-right-radius:12px;}"
        ).arg(C_CARD, C_BORDER));
        auto* ir = new QHBoxLayout(inputRow); ir->setContentsMargins(16, 0, 14, 0); ir->setSpacing(10);

        auto* promptSym = new QLabel(">>"); promptSym->setFont(MF(12, QFont::Bold));
        promptSym->setStyleSheet(QString("color:%1;background:transparent;").arg(C_ACCENT));

        m_promptLbl = new QLabel; m_promptLbl->setFont(MF(10));
        m_promptLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(C_ACCENT));
        m_promptLbl->hide();



        m_input = new DraggableExpressionEdit(this);
        m_input->setPlaceholderText("Enter expression, equation, or shape...");
        m_input->setFont(MF(10)); m_input->setFrame(false);
        m_input->setStyleSheet(QString("QLineEdit{background:transparent;border:none;color:%1;}").arg(C_TEXT));

        m_runBtn = new QPushButton("RUN"); m_runBtn->setFont(MF(9, QFont::Bold)); m_runBtn->setFixedSize(72, 32);
        m_runBtn->setStyleSheet(QString(
            "QPushButton{background:%1;border:none;color:#000;border-radius:6px;letter-spacing:1px;}"
            "QPushButton:hover{background:%2;}"
        ).arg(C_ACCENT, C_ACCENT_DIM));

        m_stopBtn = new QPushButton("STOP"); m_stopBtn->setFont(MF(9, QFont::Bold)); m_stopBtn->setFixedSize(72, 32);
        m_stopBtn->setStyleSheet(QString(
            "QPushButton{background:%1;border:none;color:#fff;border-radius:6px;letter-spacing:1px;}"
            "QPushButton:hover{background:%2;}"
        ).arg(C_VRED, C_DRED));
        m_stopBtn->hide();



        ir->addWidget(promptSym);
        ir->addWidget(m_promptLbl);
        ir->addWidget(m_input, 1);
        ir->addWidget(m_runBtn);
        ir->addWidget(m_stopBtn);


        tl->addWidget(m_termBar);
        tl->addWidget(m_output, 1);
        tl->addWidget(inputRow);

        // Construct input subsystems now that widgets exist
        m_histNav = new HistoryNavigator(m_history);
        m_promptCtrl = new PromptController(m_promptLbl, m_input, this);

        // Wire signals
        connect(m_runBtn, &QPushButton::clicked, this, &MainWindow::onRun);
        connect(m_input, &QLineEdit::returnPressed, this, &MainWindow::onRun);
        connect(m_stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
        connect(m_output, &OutputArea::calcFinish, this, &MainWindow::onCalcFinish);

        connect(m_promptCtrl, &PromptController::paramReady, this, &MainWindow::onParamReady);
        connect(m_promptCtrl, &PromptController::promptComplete, this, &MainWindow::onPromptComplete);
        connect(m_promptCtrl, &PromptController::promptCancelled, this, &MainWindow::onPromptCancelled);
        connect(m_promptCtrl, &PromptController::needsMoreInput,
            this, [this](const QString& param, const QString& reason) {
                m_output->addResultLine(QString("\xe2\x9a\xa0 %1").arg(reason), "err");
                m_output->addPromptRequest(param);
                m_input->setFocus();
            });

        connect(MathEngine::instance(), &MathEngine::simplification,
            m_output, &OutputArea::addSimplified);

        connect(m_input, &QLineEdit::textEdited, this, [this]() {
            m_histNav->reset();
            });

        m_input->installEventFilter(this);
    }

    QWidget* MainWindow::createSidebar() {
        auto* sidebar = new QWidget;
        sidebar->setFixedWidth(290);
        sidebar->setStyleSheet("background:transparent;");
        auto* sbL = new QVBoxLayout(sidebar);
        sbL->setContentsMargins(0, 0, 0, 0);
        sbL->setSpacing(12);

        // Quick Reference Frame
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
        connect(m_refPanel, &SidebarPanel::itemClicked, this, &MainWindow::onSidebarItemClicked);
        connect(m_refPanel, &SidebarPanel::itemDoubleClicked, this, &MainWindow::onSidebarItemDoubleClicked);
        refVL->addWidget(refHead);
        refVL->addWidget(m_refPanel, 1);
        sbL->addWidget(refFrame);

        // Session Frame
        auto* sessFrame = new QFrame;
        sessFrame->setObjectName("sessFrame");
        sessFrame->setStyleSheet(QString(
            "QFrame#sessFrame{background:%1;border:1px solid %2;border-radius:12px;}"
        ).arg(C_SURFACE, C_BORDER));
        auto* sessVL = new QVBoxLayout(sessFrame);
        sessVL->setContentsMargins(0, 0, 0, 0);
        sessVL->setSpacing(0);
        auto* sessHead = new QLabel("SESSION");
        sessHead->setFont(MF(8));
        sessHead->setStyleSheet(QString(
            "background:%1;border-bottom:1px solid %2;"
            "border-top-left-radius:12px;border-top-right-radius:12px;"
            "padding:9px 14px;color:%3;letter-spacing:2px;"
        ).arg(C_CARD, C_BORDER, C_MUTED));
        auto* sessBody = new QWidget;
        sessBody->setStyleSheet("background:transparent;");
        auto* sessBodyL = new QVBoxLayout(sessBody);
        sessBodyL->setContentsMargins(14, 10, 14, 12);
        sessBodyL->setSpacing(5);
        auto makeRow = [&](const QString& t, QLabel*& valLbl, const QString& valColor) {
            auto* row = new QHBoxLayout;
            row->setContentsMargins(0, 0, 0, 0);
            auto* lbl = new QLabel(t);
            lbl->setFont(MF(9));
            lbl->setStyleSheet(QString("color:%1;background:transparent;").arg(C_MUTED));
            valLbl = new QLabel("—");
            valLbl->setFont(MF(9));
            valLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(valColor));
            row->addWidget(lbl);
            row->addSpacing(6);
            row->addWidget(valLbl);
            row->addStretch();
            sessBodyL->addLayout(row);
            };
        makeRow("Calculations:", m_countLbl, C_TEXT);
        m_countLbl->setText("0");
        makeRow("Last result:", m_lastResultLbl, C_ACCENT);
        makeRow("Last Expression Ran:", m_lastExprLbl, C_ACCENT);
        m_runtimeLbl = new QLabel("");
        makeRow("Uptime:", m_runtimeLbl, C_ACCENT);
        updateRuntimeLabel(); // populate immediately, don't wait for first 1s tick
        auto* clearBtn = new QPushButton("Clear History");
        clearBtn->setFont(MF(9));
        clearBtn->setFixedHeight(26);
        clearBtn->setStyleSheet(QString(
            "QPushButton{background:none;border:1px solid %1;color:%2;padding:2px 12px;border-radius:6px;}"
            "QPushButton:hover{border-color:%3;color:%3;}"
        ).arg(C_BORDER, C_MUTED, C_ERR));
        connect(clearBtn, &QPushButton::clicked, this, &MainWindow::onClear);

        sessBodyL->addSpacing(2);
        sessBodyL->addWidget(clearBtn);
        sessVL->addWidget(sessHead);
        sessVL->addWidget(sessBody);
        sbL->addWidget(sessFrame);

        return sidebar;
    }