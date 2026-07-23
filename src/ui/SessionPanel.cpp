#include "SessionPanel.h"
#include "../constants/Theme.h"
#include "../subSystems/WidgetRegistry.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QApplication>
#include "MemoryUsage.h"

static QFont MF(int pt, int w = QFont::Normal) {
    return Theme::monoFont(pt, w);
}

SessionPanel::SessionPanel(QWidget* parent) : QFrame(parent) {
    setObjectName("sessFrame");
    setStyleSheet(QString(
        "QFrame#sessFrame{background:%1;border:1px solid %2;border-radius:12px;}"
    ).arg(C_SURFACE, C_BORDER));

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* head = new QLabel("SESSION");
    head->setFont(MF(8));
    head->setStyleSheet(QString(
        "background:%1;border-bottom:1px solid %2;"
        "border-top-left-radius:12px;border-top-right-radius:12px;"
        "padding:9px 14px;color:%3;letter-spacing:2px;"
    ).arg(C_CARD, C_BORDER, C_MUTED));
    outer->addWidget(head);

    auto* body = new QWidget;
    body->setStyleSheet("background:transparent;");
    m_bodyLayout = new QVBoxLayout(body);
    m_bodyLayout->setContentsMargins(14, 10, 14, 12);
    m_bodyLayout->setSpacing(5);

    m_countLbl      = addRow("Calculations:",       C_TEXT);
    m_lastResultLbl = addRow("Last result:",        C_ACCENT);
    m_lastExprLbl   = addRow("Last Ran:",           C_ACCENT);
    m_uptimeLbl     = addRow("Uptime:",             C_ACCENT);
    m_memUsageLbl   = addRow("Memory Usage:",       C_ACCENT);
    m_widgetCntLbl  = addRow("Widget count:",       C_ACCENT);

    m_countLbl->setText("0");

    auto* clearBtn = new QPushButton("Clear History");
    clearBtn->setFont(MF(9));
    clearBtn->setFixedHeight(26);
    clearBtn->setStyleSheet(QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;"
        "padding:2px 12px;border-radius:6px;}"
        "QPushButton:hover{border-color:%3;color:%3;}"
    ).arg(C_BORDER, C_MUTED, C_ERR));
    connect(clearBtn, &QPushButton::clicked, this, &SessionPanel::clearRequested);

    m_bodyLayout->addSpacing(2);
    m_bodyLayout->addWidget(clearBtn);
    outer->addWidget(body);

    WR_ADD(m_countLblId,      m_countLbl,      WidgetRole::Text       | WidgetRole::FontSidebar);
    WR_ADD(m_lastResultLblId, m_lastResultLbl, WidgetRole::AccentText | WidgetRole::FontSidebar);
    WR_ADD(m_lastExprLblId,   m_lastExprLbl,   WidgetRole::AccentText | WidgetRole::FontSidebar);
    WR_ADD(m_uptimeLblId,     m_uptimeLbl,     WidgetRole::AccentText | WidgetRole::FontSidebar);
    WR_ADD(m_memUsageLblId,   m_memUsageLbl,   WidgetRole::AccentText | WidgetRole::FontSidebar);
    WR_ADD(m_widgetCntLblId,  m_widgetCntLbl,  WidgetRole::AccentText | WidgetRole::FontSidebar);

    // One timer for all three polled metrics. MainWindow previously ran three
    // separate QTimers at 1000ms, one of which also wrote three qDebug lines
    // per tick for the lifetime of the process.
    m_sessionTimer.start();
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        updateUptime();
        updateMemory();
        updateWidgetCount();
    });
    m_pollTimer->start(1000);

    updateUptime();       // populate immediately; don't wait for the first tick
    updateMemory();
    updateWidgetCount();
}

QLabel* SessionPanel::addRow(const QString& caption, const QString& valueColor) {
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);

    auto* cap = new QLabel(caption);
    cap->setFont(MF(9));
    cap->setStyleSheet(QString("color:%1;background:transparent;").arg(C_MUTED));

    auto* val = new QLabel(QStringLiteral("\u2014"));   // em dash
    val->setFont(MF(9));
    val->setStyleSheet(QString("color:%1;background:transparent;").arg(valueColor));

    row->addWidget(cap);
    row->addSpacing(6);
    row->addWidget(val);
    row->addStretch();
    m_bodyLayout->addLayout(row);
    return val;
}

QString SessionPanel::elide(const QString& s, int maxChars) {
    if (s.length() <= maxChars) return s;
    return s.first(maxChars - 3) + QStringLiteral("...");
}

void SessionPanel::setCalcCount(int n) {
    m_countLbl->setText(QString::number(n));
}

void SessionPanel::setLastResult(const QString& result) {
    m_lastResultLbl->setText(elide(result));
}

void SessionPanel::setLastExpression(const QString& expr) {
    m_lastExprLbl->setText(elide(expr));
}

void SessionPanel::reset() {
    m_countLbl->setText("0");
    m_lastResultLbl->setText(QStringLiteral("\u2014"));
    m_lastExprLbl->setText(QString());
}

void SessionPanel::updateUptime() {
    const qint64 total = m_sessionTimer.elapsed() / 1000;
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;

    QString out;
    if (h > 0)          out += QString::number(h) + "h ";
    if (m > 0 || h > 0) out += QString::number(m) + "m ";
    out += QString::number(s) + "s";

    m_uptimeLbl->setText(out);
}

void SessionPanel::updateMemory() {
    // getMemoryUsage() is portable (Win: PROCESS_MEMORY_COUNTERS, Linux:
    // /proc/self/status); it returns 0 on platforms without an implementation.
    const std::size_t rss = getMemoryUsage().workingSet;
    if (rss > 0) {
        m_memUsageLbl->setText(QString("%1 MB")
            .arg(rss / (1024.0 * 1024.0), 0, 'f', 1));
    } else {
        m_memUsageLbl->setText(QStringLiteral("\u2014"));
    }
}

void SessionPanel::updateWidgetCount() {
    m_widgetCntLbl->setText(QString::number(QApplication::allWidgets().size()));
}
