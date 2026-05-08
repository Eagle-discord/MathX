#pragma once
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QFrame>
#include <QStringList>
#include "RunState.h"
#include "../input/InputAction.h"
#include "../input/HistoryNavigator.h"
#include "../input/PromptController.h"
#include "../math/BigNum.h"
#include "../thread/PersistentWorker.h"
#include <QThread>


using boost::multiprecision::cpp_int;


class OutputArea;
class GeoCard;
class SidebarPanel;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    RunState getRunState();
    ~MainWindow();
protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    void closeEvent(QCloseEvent* closeEvent) override;


private slots:
    void onRun();
    void onModeChanged(const QString& mode);
    void onClear();
    void onSidebarItemClicked(const QString& expr);
    void onSidebarItemDoubleClicked(const QString& expr);

    void setRunState(RunState state);
    void onStop();
    void onCalcFinish(RunState state, QString result);
    void onWorkerFinish(int jobId, const QString& result, const QString& type, const QString& formula);
//    void performFactorial(BigInt fac_num);

    

    // PromptController slots
    void onParamReady(const QString& param, const QString& value);
    void onPromptComplete(const QString& fullExpr);
    void onPromptCancelled();



signals:
    void stop();
    void addWidgetToLayout(QWidget* widget);

private:

    void setupUi();
    void setupHeader();
    void setupTerminal();


    void run(const QString& expr);
    bool tryStartPrompt(const QString& expr);
    void handlePromptInput(const QString& value);
    void submitExpression(const QString& expr);
    void onTruncateClicked();
    void onAsyncResult(int jobId, const QString& result, const QString& type);
    // UI widgets
    QPushButton* m_copyButton = nullptr;
    QWidget*      m_header        = nullptr;
    QWidget*      m_modesBar      = nullptr;
    QWidget*      m_terminal      = nullptr;
    OutputArea*   m_output        = nullptr;
    QLineEdit*    m_input         = nullptr;

    QPushButton*  m_runBtn        = nullptr;
    QPushButton*  m_stopBtn       = nullptr;
    QLabel*       m_promptLbl     = nullptr;
    SidebarPanel* m_refPanel      = nullptr;
    QLabel*       m_countLbl      = nullptr;
    QLabel*       m_lastExprLbl   = nullptr;
    QLabel*       m_lastResultLbl = nullptr;
    QPushButton*  m_activeMode    = nullptr;
    QPushButton*  m_truncate_toggle   = nullptr;
    // Application state

    RunState    m_state       = RunState::Idle;
    QStringList m_history;
    int         m_calcCount = 0;
    QThread* m_workerThread = nullptr;
    PersistentWorker* m_worker = nullptr;
    QMap<int, QString> m_pendingJobs;
    int m_nextJobId = 0;
    QString     m_lastResult;
    QString     m_currentMode = "all";

    // Input subsystems
    HistoryNavigator* m_histNav    = nullptr;
    PromptController* m_promptCtrl = nullptr;
};
