#pragma once
#include <QFrame>
#include <QElapsedTimer>

class QLabel;
class QTimer;

// -- SessionPanel --------------------------------------------------------------
// The "SESSION" card in the sidebar: calc count, last result, last expression,
// uptime, memory, live widget count, and the Clear History button.
//
// Extracted from MainWindow::createSidebar(), which was constructing widgets,
// polling Win32 process memory, and qDebug()-ing three metrics per second from
// inside a UI-building function.
class SessionPanel : public QFrame {
    Q_OBJECT
public:
    explicit SessionPanel(QWidget* parent = nullptr);

    void setCalcCount(int n);
    void setLastResult(const QString& result);
    void setLastExpression(const QString& expr);
    void reset();

signals:
    void clearRequested();

private:
    QLabel* addRow(const QString& caption, const QString& valueColor);
    void updateUptime();
    void updateMemory();
    void updateWidgetCount();

    // Sidebar values are elided, not wrapped -- the panel is a fixed 290px and
    // a 1786-digit factorial would otherwise stretch it off-screen.
    static QString elide(const QString& s, int maxChars = 23);

    QLabel* m_countLbl     = nullptr;
    QLabel* m_lastResultLbl = nullptr;
    QLabel* m_lastExprLbl  = nullptr;
    QLabel* m_uptimeLbl    = nullptr;
    QLabel* m_memUsageLbl  = nullptr;
    QLabel* m_widgetCntLbl = nullptr;

    class QVBoxLayout* m_bodyLayout = nullptr;

    // Monotonic -- immune to system clock changes (sleep, resume, manual
    // adjustment) in a way that a QDateTime diff would not be.
    QElapsedTimer m_sessionTimer;
    QTimer* m_pollTimer = nullptr;   // one timer, three metrics

    int m_countLblId      = -1;
    int m_lastResultLblId = -1;
    int m_lastExprLblId   = -1;
    int m_uptimeLblId     = -1;
    int m_memUsageLblId   = -1;
    int m_widgetCntLblId  = -1;
};
