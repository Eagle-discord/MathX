#pragma once
#include "GhostLineEdit.h"
#include "DemoTyper.h"
#include <QMainWindow>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMargins>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QStringList>
#include <QThread>

#include "../constants/RunState.h"
#include "../constants/RunSource.h"
#include "../constants/StateUtils.h"
#include "../input/HistoryNavigator.h"
#include "../input/InputAction.h"
#include "../input/PromptController.h"
#include "../thread/PersistentWorker.h"

class QPropertyAnimation;
class BatchPanel;
class BatchRunner;
class CopyableLabel;
class FeatureViewerPage;
class GeoCard;
class GeoModeWidget;
class ModdedBtn;
class OutputArea;
class SessionPanel;
class SettingsPage;
class SidebarPanel;
class WidgetEditorPage;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    RunState getRunState() const { return m_state; }

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onRun();
    void onStop();
    void onClear();
    void onCopyHistory();
    void onModeChanged(const QString& mode);
    void onSidebarItemClicked(const QString& expr);
    void onSidebarItemDoubleClicked(const QString& expr);

    // Submits an expression programmatically as if the user pressed Enter.
    // Sets RunSource::App so submitExpression skips the UI noise (history nav,
    // counter increment, addInputLine) -- only the worker pipeline runs.
    void runExpressionInternally(const QString& expr);

    void onShowGeometryMode(const QString& type, const QMap<QString, double>& params);
    void destroyGeometryMode();
    void onShowWidgetEditor(QWidget* target);
    void destroyWidgetEditor();

    void setRunState(RunState state);
    void onWorkerFinish(int jobId, const QString& result, ResultType type,
        const QString& formula, double calcTime);

    // Re-runs `expr` through the worker, then rewrites `target` in place
    // instead of appending a new result line. Used by the scrub / inline-edit
    // feature on the input line.
    void onExpressionRecompute(const QString& expr, CopyableLabel* target);

    // PromptController slots
    void onParamReady(const QString& param, const QString& value);
    void onPromptComplete(const QString& fullExpr);
    void onPromptCancelled();

signals:
    void stop();
    void jobFinished(const QString& result, ResultType type);

private:
    // -- Construction ----------------------------------------------------------
    void setupUi();
    void setupHeader();
    void setupTerminal();
    QWidget* createSidebar();
    void pulsePromptBtn();   // draw the eye to the '>>' batch toggle
    void ensureSettingsPage();
    void ensureFeaturePage();
    void wireSettings();
    void wireWorker();
    void wireBatch();

    // -- Pipeline --------------------------------------------------------------
    void submitExpression(const QString& expr);
    void run(const QString& expr);
    bool tryStartPrompt(const QString& expr);
    void handlePromptInput(const QString& value);
    void applySettings();

    // onWorkerFinish branches, one per result type
    void handleGeoResult(const QString& expr, double calcTime);
    void handleRetarget(int jobId, const QString& result,
        ResultType type, double calcTime);
    void finishJob();

    void appendTiming(double calcTime);

    // -- Pages -----------------------------------------------------------------
    QStackedWidget* m_centralStack = nullptr;
    QWidget* m_terminalPage = nullptr;
    QWidget* m_settingsPage = nullptr;
    SettingsPage* m_settingsPageWidget = nullptr;
    GeoModeWidget* m_geoModeWidget = nullptr;
    WidgetEditorPage* m_widgetEditorPage = nullptr;
    FeatureViewerPage* m_featurePage = nullptr;

    QMargins m_originalMargins;
    int      m_originalSpacing = 0;

    // -- Header ----------------------------------------------------------------
    QWidget* m_header = nullptr;
    QWidget* m_modesBar = nullptr;
    QPushButton* m_activeMode = nullptr;
    QPushButton* m_settingsBtn = nullptr;
    QPushButton* m_settingsBackBtn = nullptr;
    QPushButton* m_featuresBtn = nullptr;

    // -- Terminal --------------------------------------------------------------
    QWidget* m_terminal = nullptr;
    QWidget* m_termBar = nullptr;
    QLabel* m_termTitle = nullptr;
    QFrame* m_inputRow = nullptr;
    OutputArea* m_output = nullptr;
    GhostLineEdit* m_input = nullptr;
    DemoTyper* m_demo = nullptr;   // self-playing example reel; see DemoTyper.h
    QLabel* m_promptLbl = nullptr;
    QPushButton* m_runBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_copyHistBtn = nullptr;
    ModdedBtn* m_promptBtn = nullptr;
    // Non-owning: the animation is parented to m_promptBtn and self-deletes on
    // stop. Held only so a second pulse can cancel the first.
    QPropertyAnimation* m_promptPulse = nullptr;

    // -- Sidebar ---------------------------------------------------------------
    SidebarPanel* m_refPanel = nullptr;
    SessionPanel* m_sessionPanel = nullptr;

    // -- Batch -----------------------------------------------------------------
    BatchPanel* m_batchPanel = nullptr;
    BatchRunner* m_batchRunner = nullptr;
    bool m_batchRunning = false;

    // -- Worker ----------------------------------------------------------------
    QThread* m_workerThread = nullptr;
    PersistentWorker* m_worker = nullptr;
    QMap<int, QString> m_pendingJobs;
    int m_nextJobId = 0;

    // jobId -> the label that job must update in place. Present only for jobs
    // originated by a scrub / inline edit. QPointer so a clearAll() that deletes
    // the widget mid-flight leaves us a null, not a dangler.
    QMap<int, QPointer<CopyableLabel>> m_retargetJobs;

    // -- State -----------------------------------------------------------------
    RunState    m_state = RunState::Idle;
    QStringList m_history;
    QStringList m_copyHistBuffer;
    int         m_calcCount = 0;
    QString     m_lastResult;
    QString     m_currentMode = "all";

    // -- Input subsystems ------------------------------------------------------
    HistoryNavigator* m_histNav = nullptr;
    PromptController* m_promptCtrl = nullptr;

    // -- WidgetRegistry ids ----------------------------------------------------
    int m_runBtnId = -1;
    int m_stopBtnId = -1;
    int m_inputId = -1;
    int m_promptBtnId = -1;
    int m_termTitleId = -1;
    int m_copyHistBtnId = -1;
    int m_settingsBtnId = -1;
    int m_settingsBackBtnId = -1;
    int m_featuresBtnId = -1;
};