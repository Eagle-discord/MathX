#include "OutputWidget.h"
#include <constants/Theme.h>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>

OutputWidget::OutputWidget(const QString& displayText, const QString& copyText,
    const QString& originalExpr, const QString& formula,
    const QString& type, const QString& color,
    const QString& extraStyle, QWidget* parent)
    : QLabel(displayText, parent), m_displayText(displayText), m_copyText(copyText),
    m_originalExpr(originalExpr), m_formula(formula), m_type(type), m_color(color)
{
    setCursor(Qt::PointingHandCursor);
    setTextInteractionFlags(Qt::NoTextInteraction);
    setWordWrap(true);
    setFont(Theme::monoFont(9));
    setStyleSheet(QString("background:transparent; color:%1; %2;padding-left:22px").arg(m_color).arg(extraStyle));
    m_copyResetTimer.setSingleShot(true);
    connect(&m_copyResetTimer, &QTimer::timeout, this, &OutputWidget::resetCopyState);
}

void OutputWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton) {
        copyToClipboard();
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void OutputWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && (m_type == "conv" || m_type == "alg") && !m_formula.isEmpty()) {
        m_formulaVisible = !m_formulaVisible;
        if (m_formulaVisible) {
            setText(m_displayText + "  [ " + m_formula + " ]");
            setStyleSheet(QString("background:transparent; color:%1;padding-left:22px").arg(Theme::INFO)); // optional highlight
        }
        else {
            setText(m_displayText);
            setStyleSheet(QString("background:transparent; color:%1;padding-left:22px").arg(m_color));
        }
        event->accept();
        return;
    }
    QLabel::mouseDoubleClickEvent(event);
}

void OutputWidget::enterEvent(QEnterEvent* event) {
    if (!m_copied && (m_type == "ok" || m_type == "conv" || m_type == "alg")) {
        setText(m_displayText + "  \u2702 Copy");
    }
    QLabel::enterEvent(event);
}

void OutputWidget::leaveEvent(QEvent* event) {
    if (!m_copied) {
        setText(m_displayText);
    }
    QLabel::leaveEvent(event);
}

void OutputWidget::copyToClipboard() {
    QApplication::clipboard()->setText(m_copyText);
    m_copied = true;
    setText("Copied!");
    setStyleSheet(QString("background:transparent; color:%1;").arg(Theme::ACCENT)); // temporary feedback
    m_copyResetTimer.start(800);
}

void OutputWidget::resetCopyState() {
    m_copied = false;
    setText(m_displayText);
    setStyleSheet(QString("background:transparent; color:%1;").arg(m_color));
}