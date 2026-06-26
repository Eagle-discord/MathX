#pragma once
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QFrame>
#include <QStringList>
#include "..\constants\RunState.h"
#include "..\constants\StateUtils.h"
#include "..\constants\RunSource.h"
#include "../input/InputAction.h"
#include "../input/HistoryNavigator.h"
#include "../input/PromptController.h"
#include "../math/BigNum.h"
#include "../thread/PersistentWorker.h"
#include <QThread>
#include <QElapsedTimer>
#include <QDateTime>
#include "HistoryDock.h"
#include "FocusGlow.h"
#include "FocusAnchor.h"
#include "settings/SettingsPage.h"
#include "GeoModeWidget.h"
#include <QStackedWidget>

using boost::multiprecision::cpp_int;

using VarMap = QMap<QString, double>;
class OutputArea;
class GeoCard;
class SidebarPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    void onShowProjection(const QString& type, const QMap<QString, double>& params);

    RunState getRunState();
    ~MainWindow();
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* closeEvent) override;
    void resizeEvent(QResizeEvent* resizeEvent) override;

private slots:
    void onRun();
    // Submits an expression programmatically as if the user pressed Enter.
    // Sets RunSource::App so submitExpression skips UI noise (history nav,
    // counter increment, addInputLine) — only the worker pipeline runs.
    void runExpressionInternally(const QString& expr);
    void onModeChanged(const QString& mode);
    void onClear();
    void onSidebarItemClicked(const QString& expr);
    void onSidebarItemDoubleClicked(const QString& expr);
    void onShowGeometryMode(const QString& type, const QMap<QString, double>& params);

    void setRunState(RunState state);
    void onStop();
    void onCalcFinish(RunState state, QString result);
    void onWorkerFinish(int jobId, const QString& result, const QString& type, const QString& formula);
    //    void performFactorial(BigInt fac_num);
    void onCopyHistory();


    // PromptController slots
    void onParamReady(const QString& param, const QString& value);
    void onPromptComplete(const QString& fullExpr);
    void onPromptCancelled();



signals:
    void stop();
    void addWidgetToLayout(QWidget* widget);

private:
    QMargins m_originalMargins;
    int m_originalSpacing;
    QStackedWidget* m_centralStack = nullptr;
    GeoModeWidget* m_geoModeWidget = nullptr;
    QWidget* m_settingsPage = nullptr;
    QPushButton* m_settingsBtn = nullptr;   // gear button in terminal header
    QPushButton* m_settingsBackBtn = nullptr;   // >_ button on settings page
    SettingsPage* m_settingsPageWidget = nullptr;   // the actual settings UI
    void setupUi();
    void setupHeader();
    void setupTerminal();
    QWidget* createSidebar();
    void recreateGeometryMode();
    void run(const QString& expr);
    bool tryStartPrompt(const QString& expr);
    void handlePromptInput(const QString& value);
    void submitExpression(const QString& expr);
    void onTruncateClicked();
    void onAsyncResult(int jobId, const QString& result, const QString& type);
    // UI widgets
    //HistoryDock*  m_historyDock;
    FocusGlow* m_focusGlow;
    FocusAnchor* m_focusAnchor = nullptr;
    QPushButton* m_copyButton = nullptr;
    QWidget* m_header = nullptr;
    QWidget* m_modesBar = nullptr;
    QWidget* m_terminal = nullptr;
    QWidget* m_termBar = nullptr;
    QWidget* m_termTitle = nullptr;
    QFrame* m_inputRow = nullptr;  // holds the glow QGraphicsDropShadowEffect
    OutputArea* m_output = nullptr;
    QLineEdit* m_input = nullptr;
    VarMap m_promptVars;
    QPushButton* m_runBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QLabel* m_promptLbl = nullptr;
    SidebarPanel* m_refPanel = nullptr;
    QLabel* m_countLbl = nullptr;
    QLabel* m_lastExprLbl = nullptr;
    QLabel* m_lastResultLbl = nullptr;
    QPushButton* m_activeMode = nullptr;
    QPushButton* m_copyHistBtn = nullptr;
    QPushButton* m_truncate_toggle = nullptr;
    // Application state

    RunState    m_state = RunState::Idle;
    QStringList m_history;
    QStringList m_copyHistBuffer;
    int         m_calcCount = 0;
    QThread* m_workerThread = nullptr;

    // -- Session uptime ---------------------------------------------------------
    // QElapsedTimer is monotonic — immune to system clock changes (sleep,
    // resume, manual clock adjustment) unlike QDateTime::currentDateTime()
    // diffs would be. Single source of truth for "how long has this run".
    QElapsedTimer m_sessionTimer;
    QTimer* m_runtimeUpdateTimer = nullptr; // ticks every second, refreshes m_runtimeLbl
    QLabel* m_runtimeLbl = nullptr;
    void updateRuntimeLabel();
    PersistentWorker* m_worker = nullptr;
    QMap<int, QString> m_pendingJobs;
    QMap<int, int> m_streamingJobs;  // jobId → streamId for active streams
    int m_nextJobId = 0;
    QString     m_lastResult;
    QString     m_currentMode = "all";

    // Input subsystems
    HistoryNavigator* m_histNav = nullptr;
    PromptController* m_promptCtrl = nullptr;
};