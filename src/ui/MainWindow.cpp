#include "MainWindow.h"
#include "OutputArea.h"
#include "SidebarPanel.h"
#include "../math/MathEngine.h"
#include "../input/InputHandler.h"
#include "../shapes/Shapes2D.h"
#include "../shapes/Shapes3D.h"
#include <QThread>
#include "../math/FactorialWorker.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFrame>
#include <QFont>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QScrollArea>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QRegularExpression>
#include <QMessageBox>
#include "../theme/Theme.h"
#include "../input/InputRouter.h"

static const QString C_BG = Theme::BG;
static const QString C_SURFACE = Theme::SURFACE;
static const QString C_CARD = Theme::CARD;
static const QString C_BORDER = Theme::BORDER;
static const QString C_ACCENT = Theme::ACCENT;
static const QString C_ACCENT_DIM = Theme::ACCENT_DIM;
static const QString C_TEXT = Theme::TEXT;
static const QString C_MUTED = Theme::MUTED;
static const QString C_ERR = Theme::ERROR;
static const QString C_VRED = "#dc1e14";
static const QString C_DRED = "#ad102f";

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

// ─────────────────────────────────────────────────────────────────────────────
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("MATHX \xe2\x80\x94 Unlimited Calculator");
    setMinimumSize(900, 640); resize(1100, 750);
    setStyleSheet(QString("QMainWindow,QWidget{background:%1;color:%2;}").arg(C_BG, C_TEXT));
    setupUi();
}

// ── eventFilter ───────────────────────────────────────────────────────────────
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

// ── setRunState ───────────────────────────────────────────────────────────────
void MainWindow::setRunState(RunState state) {
    m_state = state;
    switch (state) {
    case RunState::Idle:
        m_stopBtn->hide();
        m_runBtn->show();
        m_input->setEnabled(true);
        if (!m_promptCtrl->isActive())
            m_input->setPlaceholderText("Enter expression, equation, or shape...");
        break;
    case RunState::Running:
        m_stopBtn->show();
        m_runBtn->hide();
        m_input->setEnabled(false);
        break;
    case RunState::HandlingInput:
        m_stopBtn->show();
        m_runBtn->hide();
        m_input->setEnabled(true);
        break;
    case RunState::Stopping:
        emit stop();
        break;
    }
}

// ── submitExpression ──────────────────────────────────────────────────────────
void MainWindow::submitExpression(const QString& expr) {
    m_output->addInputLine(expr);
    m_lastExprLbl->setText(expr);
    m_calcCount++;
    m_countLbl->setText(QString::number(m_calcCount));
    m_histNav->reset();
    setRunState(RunState::Running);
    run(expr);
}

// ── onRun ─────────────────────────────────────────────────────────────────────
void MainWindow::onRun() {
    QString text = m_input->text().trimmed();
    if (text.isEmpty()) return;
    m_input->clear();

    if (m_promptCtrl->isActive()) {
        handlePromptInput(text);
        return;
    }
    if (!tryStartPrompt(text)) {
        setRunState(RunState::HandlingInput);
        submitExpression(text);
    }
}

// ── run ───────────────────────────────────────────────────────────────────────
void MainWindow::run(const QString& expr) {
    m_history.append(expr);
    m_lastExprLbl->setText(expr);

    // Factorial — async path
    static QRegularExpression factRe(
        R"(^\s*fact\s*\(\s*(-?\d+)\s*\)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto match = factRe.match(expr);
    if (match.hasMatch()) {
        BigInt n(match.captured(1).toStdString());
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
        if (res.type == "geo") {
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
                    m_promptCtrl->start(prompt);
                    setRunState(RunState::HandlingInput);
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
}

// ── onCalcFinish ──────────────────────────────────────────────────────────────
void MainWindow::onCalcFinish(RunState state, QString result) {
    setRunState(state);
    m_lastResult = result;
    m_lastResultLbl->setText(
        result.length() > 23 ? result.first(20) + "..." : result);
}

// ── Factorial ─────────────────────────────────────────────────────────────────
void MainWindow::onFactorialProgress(cpp_int percent) {
    m_output->showProgress((int)percent);
}

void MainWindow::onFactorialFinished(const QString& result) {
    m_output->hideProgress();
    const int fullLen = result.length();
    const int limit = 500;
    if (fullLen > limit) {
        QString display = result.left(limit);
        display.append(QString("\n.... %1 more characters (%2 total)")
            .arg(fullLen - limit).arg(fullLen));
        m_output->addResultLine(display, "big");
    }
    else {
        m_output->addResultLine(result, "big");
    }
    m_output->addSeparator();
    setRunState(RunState::Idle);
}

void MainWindow::computeFactorialAsync(BigInt n) {
    auto* worker = new FactorialWorker;
    auto* thread = new QThread;
    m_output->showProgress(0);
    worker->moveToThread(thread);
    connect(worker, &FactorialWorker::progress,
        this, &MainWindow::onFactorialProgress, Qt::QueuedConnection);
    connect(this, &MainWindow::stop, thread, &QThread::requestInterruption);
    connect(this, &MainWindow::stop, worker, &FactorialWorker::InterruptionRequest);
    connect(worker, &FactorialWorker::finished,
        this, &MainWindow::onFactorialFinished, Qt::QueuedConnection);
    connect(thread, &QThread::started, worker, [worker, n]() {
        worker->computeFactorial(n);
        });
    connect(worker, &FactorialWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

// ── onStop ────────────────────────────────────────────────────────────────────
void MainWindow::onStop() {
    setRunState(RunState::Stopping);
    emit stop();
}

// ── onTruncateClicked ─────────────────────────────────────────────────────────
void MainWindow::onTruncateClicked() {
    // Toggle truncation behaviour — placeholder for future implementation
}

// ── PromptController slots ────────────────────────────────────────────────────
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
    m_output->addResultLine("  cancelled", "err");
    m_output->addSeparator();
    setRunState(RunState::Idle);
}

// ── handlePromptInput ─────────────────────────────────────────────────────────
void MainWindow::handlePromptInput(const QString& value) {
    setRunState(RunState::HandlingInput);
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

    if (!ok) {
        m_output->addResultLine(
            QString("\xe2\x9a\xa0 Invalid input for '%1'").arg(param), "err");
        m_output->addPromptRequest(param);
        m_input->setFocus();
        return;
    }

    m_promptCtrl->submit(value, numericValue);
 
}

// ── tryStartPrompt ────────────────────────────────────────────────────────────
bool MainWindow::tryStartPrompt(const QString& expr) {
    setRunState(RunState::HandlingInput);
    ShapePrompt p = InputHandler::detectPrompt(expr);
    if (!p.isActive()) return false;
    m_promptCtrl->start(p);
    m_output->addPromptRequest(m_promptCtrl->currentParam());

    return true;
}

// ── Mode / sidebar ────────────────────────────────────────────────────────────
void MainWindow::onModeChanged(const QString& mode) {
    m_currentMode = mode;
    m_refPanel->renderMode(mode);
}

void MainWindow::onSidebarItemClicked(const QString& expr) {
    m_input->setText(expr);
    m_input->setFocus();
}

void MainWindow::onSidebarItemDoubleClicked(const QString& expr) {
    submitExpression(expr);
    m_input->clear();
    m_input->setFocus();
}

// ── onClear ───────────────────────────────────────────────────────────────────
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

// ── setupUi ───────────────────────────────────────────────────────────────────
void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    central->setStyleSheet(QString("background:%1;").arg(C_BG));
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(24, 20, 24, 24); root->setSpacing(16);

    setupHeader(); root->addWidget(m_header);

    auto* mainWrap = new QWidget; mainWrap->setStyleSheet("background:transparent;");
    auto* mainL = new QHBoxLayout(mainWrap);
    mainL->setContentsMargins(0, 0, 0, 0); mainL->setSpacing(16);

    setupTerminal(); mainL->addWidget(m_terminal, 1);

    auto* sidebar = new QWidget; sidebar->setFixedWidth(290);
    sidebar->setStyleSheet("background:transparent;");
    auto* sbL = new QVBoxLayout(sidebar);
    sbL->setContentsMargins(0, 0, 0, 0); sbL->setSpacing(12);

    auto* refFrame = new QFrame; refFrame->setObjectName("refFrame");
    refFrame->setStyleSheet(QString(
        "QFrame#refFrame{background:%1;border:1px solid %2;border-radius:12px;}"
    ).arg(C_SURFACE, C_BORDER));
    auto* refVL = new QVBoxLayout(refFrame);
    refVL->setContentsMargins(0, 0, 0, 0); refVL->setSpacing(0);
    auto* refHead = new QLabel("QUICK REFERENCE"); refHead->setFont(MF(8));
    refHead->setStyleSheet(QString(
        "background:%1;border-bottom:1px solid %2;"
        "border-top-left-radius:12px;border-top-right-radius:12px;"
        "padding:9px 14px;color:%3;letter-spacing:2px;"
    ).arg(C_CARD, C_BORDER, C_MUTED));
    m_refPanel = new SidebarPanel;
    connect(m_refPanel, &SidebarPanel::itemClicked, this, &MainWindow::onSidebarItemClicked);
    connect(m_refPanel, &SidebarPanel::itemDoubleClicked, this, &MainWindow::onSidebarItemDoubleClicked);
    refVL->addWidget(refHead); refVL->addWidget(m_refPanel, 1);
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

// ── setupHeader ───────────────────────────────────────────────────────────────
void MainWindow::setupHeader() {
    m_header = new QWidget; m_header->setStyleSheet("background:transparent;");
    auto* hl = new QHBoxLayout(m_header); hl->setContentsMargins(0, 0, 0, 0);
    initFont();
    auto* logoArea = new QWidget; logoArea->setStyleSheet("background:transparent;");
    auto* logoL = new QVBoxLayout(logoArea); logoL->setContentsMargins(0, 0, 0, 0); logoL->setSpacing(1);
    auto* logo = new QLabel;
    QFont lf("Syne"); lf.setPointSize(20); lf.setWeight(QFont::ExtraBold);
    if (!QFontDatabase::families().contains("Syne")) lf.setFamily(g_fontFamily);
    logo->setFont(lf); logo->setTextFormat(Qt::RichText);
    logo->setText(QString("<span style='color:%1;'>MATH</span><span style='color:%2;'>X</span>")
        .arg(C_ACCENT, C_TEXT));
    auto* glow = new QGraphicsDropShadowEffect;
    glow->setBlurRadius(18); glow->setColor(QColor("#00e87a")); glow->setOffset(0, 0);
    logo->setGraphicsEffect(glow); logo->setStyleSheet("background:transparent;");
    auto* tagline = new QLabel("UNLIMITED CALCULATOR"); tagline->setFont(MF(7));
    tagline->setStyleSheet(QString("color:%1;background:transparent;letter-spacing:2px;").arg(C_MUTED));
    logoL->addWidget(logo); logoL->addWidget(tagline);
    hl->addWidget(logoArea); hl->addStretch();

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
}

// ── setupTerminal ─────────────────────────────────────────────────────────────
void MainWindow::setupTerminal() {
    m_terminal = new QFrame; m_terminal->setObjectName("terminal");
    m_terminal->setStyleSheet(QString(
        "QFrame#terminal{background:%1;border:1px solid %2;border-radius:12px;}"
    ).arg(C_SURFACE, C_BORDER));
    auto* tl = new QVBoxLayout(m_terminal); tl->setContentsMargins(0, 0, 0, 0); tl->setSpacing(0);

    auto* termBar = new QWidget; termBar->setObjectName("termBar"); termBar->setFixedHeight(36);
    termBar->setStyleSheet(QString(
        "QWidget#termBar{background:%1;border-bottom:1px solid %2;"
        "border-top-left-radius:12px;border-top-right-radius:12px;}"
    ).arg(C_CARD, C_BORDER));
    auto* tbL = new QHBoxLayout(termBar); tbL->setContentsMargins(14, 0, 16, 0); tbL->setSpacing(7);
    for (const QString& col : { "#ff5f57","#febc2e","#28c840" }) {
        auto* d = new QLabel; d->setFixedSize(12, 12);
        d->setStyleSheet(QString("background:%1;border-radius:6px;").arg(col));
        tbL->addWidget(d);
    }
    tbL->addStretch();
    auto* termTitle = new QLabel("mathx v1.0 \xe2\x80\x94 session"); termTitle->setFont(MF(8));
    termTitle->setStyleSheet(QString("color:%1;background:transparent;letter-spacing:1px;").arg(C_MUTED));
    tbL->addWidget(termTitle);

    m_output = new OutputArea;

    auto* inputRow = new QWidget; inputRow->setObjectName("inputRow"); inputRow->setFixedHeight(52);
    inputRow->setStyleSheet(QString(
        "QWidget#inputRow{background:%1;border-top:1px solid %2;"
        "border-bottom-left-radius:12px;border-bottom-right-radius:12px;}"
    ).arg(C_CARD, C_BORDER));
    auto* ir = new QHBoxLayout(inputRow); ir->setContentsMargins(16, 0, 14, 0); ir->setSpacing(10);

    auto* promptSym = new QLabel(">>"); promptSym->setFont(MF(12, QFont::Bold));
    promptSym->setStyleSheet(QString("color:%1;background:transparent;").arg(C_ACCENT));

    m_promptLbl = new QLabel; m_promptLbl->setFont(MF(10));
    m_promptLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(C_ACCENT));
    m_promptLbl->hide();

    m_input = new QLineEdit;
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

    tl->addWidget(termBar);
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