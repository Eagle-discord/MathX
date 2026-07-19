#include "DraggableResultLabel.h"
#include "../math/MathEngine.h"
#include <cstdlib>
#include <QString>
#include <QMouseEvent>
#include <QRegularExpression>
#include <cmath>

DraggableResultLabel::DraggableResultLabel(const QString& displayText, const QString& plainNumber,
    const QString& originalExpr, QWidget* parent)
    : QLabel(displayText, parent), m_originalExpr(originalExpr), m_currentDisplay(displayText),
    m_originalNumber(plainNumber)
{
    setTextInteractionFlags(Qt::NoTextInteraction);
    setCursor(Qt::PointingHandCursor);
    bool ok;
    m_originalValue = plainNumber.toDouble(&ok);
}

void DraggableResultLabel::updateResult(const QString& newDisplayText, const QString& newPlainNumber,
    const QString& newOriginalExpr) {
    setText(newDisplayText);
    m_currentDisplay = newDisplayText;
    m_originalNumber = newPlainNumber;
    m_originalExpr = newOriginalExpr;
    bool ok;
    m_originalValue = newPlainNumber.toDouble(&ok);
}

void DraggableResultLabel::rebuildExpression(double newVal) {
    QString newNumStr = QString::number(newVal, 'g', 10);
    QString newExpr = m_originalExpr;
    // Build a regex that matches the exact number as a whole token
    // Allow optional leading '+' or '-', and ensure it's not part of a larger number
    QString pattern = "(?<=[^\\d.])" + QRegularExpression::escape(m_originalNumber) + "(?=[^\\d.]|$)";
    // But simpler: use word boundaries if the number contains only digits and dot
    // For numbers with decimal, word boundary works because '.' is not a word char.
    QRegularExpression numRe("\\b" + QRegularExpression::escape(m_originalNumber) + "\\b");
    // If number starts with a sign, we need to handle it
    if (m_originalNumber.startsWith('+') || m_originalNumber.startsWith('-')) {
        // Include sign in pattern
        pattern = "\\b" + QRegularExpression::escape(m_originalNumber) + "\\b";
        numRe = QRegularExpression(pattern);
    }
    newExpr.replace(numRe, newNumStr);


    // Update stored data
    m_originalNumber = newNumStr;
    m_originalValue = newVal;
    m_originalExpr = newExpr;
    m_currentDelta = 0.0;

    // Re-evaluate the new expression
    bool ok;
    double result = MathEngine::evalSimple(newExpr, ok);
    if (ok && std::isfinite(result)) {
        QString resultStr = QString::number(result, 'g', 10);
        setText(resultStr);
        m_currentDisplay = resultStr;
        emit resultChanged(newExpr, resultStr);
    }
    else {
        setText("Error");
        m_currentDisplay = "Error";
    }
}

void DraggableResultLabel::showFloatingResult(const QString& result) {
    if (!m_floatingLabel) {
        m_floatingLabel = new QLabel(nullptr, Qt::ToolTip);
        m_floatingLabel->setWindowFlags(Qt::ToolTip);
        m_floatingLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_floatingLabel->setStyleSheet("background: #1e1e1e; color: #00e87a; border: 1px solid #00e87a; border-radius: 4px; padding: 2px 6px;");
        m_floatingLabel->setFont(QFont("Consolas", 9));
    }
    m_floatingLabel->setText("Result: " + result);
    m_floatingLabel->adjustSize();
    QPoint cursorPos = QCursor::pos();
    m_floatingLabel->move(cursorPos.x() + 10, cursorPos.y() - 20);
    m_floatingLabel->show();
}

void DraggableResultLabel::hideFloatingResult() {
    if (m_floatingLabel) m_floatingLabel->hide();
}

void DraggableResultLabel::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_lastGlobalPos = event->globalPos();
        m_currentDelta = 0.0;
        setCursor(Qt::SizeHorCursor);
        event->accept();
        return;
    }
    QLabel::mousePressEvent(event);
}

void DraggableResultLabel::mouseMoveEvent(QMouseEvent* event) {
    if (m_dragging) {
        QPoint deltaPos = event->globalPos() - m_lastGlobalPos;
        int pixelDelta = deltaPos.x();
        if (pixelDelta == 0) return;

        // Adjust step size based on modifier keys and magnitude
        double step = 1.0;
        if (event->modifiers() & Qt::ControlModifier) step = 0.1;
        else if (event->modifiers() & Qt::ShiftModifier) step = 10.0;
        double absVal = std::abs(m_originalValue);
        if (absVal > 1000) step *= 100;
        else if (absVal > 100) step *= 10;
        else if (absVal < 1 && absVal > 0.1) step = 0.1;
        else if (absVal < 0.1) step = 0.01;

        double deltaNumber = pixelDelta * step / 10.0;
        m_currentDelta += deltaNumber;
        double newVal = m_originalValue + m_currentDelta;

        rebuildExpression(newVal);
        showFloatingResult(QString::number(newVal, 'g', 10));

        m_lastGlobalPos = event->globalPos();
        event->accept();
        return;
    }
    QLabel::mouseMoveEvent(event);
}

void DraggableResultLabel::mouseReleaseEvent(QMouseEvent* event) {
    if (m_dragging) {
        m_dragging = false;
        setCursor(Qt::PointingHandCursor);
        hideFloatingResult();
        event->accept();
        return;
    }
    QLabel::mouseReleaseEvent(event);
}