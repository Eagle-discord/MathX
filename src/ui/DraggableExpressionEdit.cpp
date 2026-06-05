#include "DraggableExpressionEdit.h"
#include "../math/MathEngine.h"
#include <QMouseEvent>
#include <QApplication>
#include <QWidget>
#include <QHBoxLayout>

DraggableExpressionEdit::DraggableExpressionEdit(QWidget* parent) : QLineEdit(parent) {
    setCursor(Qt::IBeamCursor);
    m_floatingLabel = nullptr;
}

bool DraggableExpressionEdit::findNumberAtPos(int pos, int& start, int& end, double& value) {
    QString text = this->text();
    if (pos < 0 || pos >= text.length()) return false;

    // Scan left to find start of number
    start = pos;
    while (start > 0) {
        QChar ch = text[start - 1];
        if (ch.isDigit() || ch == '.' || (ch == '-' && start == 1) || (ch == '-' && text[start - 2].isSpace())) {
            start--;
        }
        else break;
    }
    // Scan right to find end
    end = pos;
    while (end < text.length()) {
        QChar ch = text[end];
        if (ch.isDigit() || ch == '.' || (ch == '-' && end == start)) {
            end++;
        }
        else break;
    }
    QString numStr = text.mid(start, end - start);
    bool ok;
    value = numStr.toDouble(&ok);
    return ok;
}

void DraggableExpressionEdit::updateNumber(int start, int end, double delta) {
    QString text = this->text();
    QString numStr = text.mid(start, end - start);
    bool ok;
    double oldVal = numStr.toDouble(&ok);
    if (!ok) return;
    double newVal = oldVal + delta;
    // Format without unnecessary trailing zeros
    QString newNum = QString::number(newVal, 'g', 10);
    // Replace the range
    QString newText = text.left(start) + newNum + text.mid(end);
    setText(newText);
    // Move cursor to the end of the changed number
    setCursorPosition(start + newNum.length());
}

void DraggableExpressionEdit::showFloatingResult(const QString& result) {
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

void DraggableExpressionEdit::hideFloatingResult() {
    if (m_floatingLabel) {
        m_floatingLabel->hide();
    }
}
void DraggableExpressionEdit::mousePressEvent(QMouseEvent* event) {
    
    if (event->button() == Qt::LeftButton) {
        int pos = cursorPositionAt(event->pos());
        if (findNumberAtPos(pos, m_dragStart, m_dragEnd, m_originalValue)) {
            m_dragging = true;
            setSelection(m_dragStart, m_dragEnd - m_dragStart); // highlight the number
            m_lastGlobalPos = event->globalPos();
            m_currentDelta = 0.0;
            setCursor(Qt::SizeHorCursor);
            event->accept();
            return;
        }
    }

    QLineEdit::mousePressEvent(event);
}

void DraggableExpressionEdit::mouseMoveEvent(QMouseEvent* event) {
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
        QString newNum = QString::number(newVal, 'g', 10);
        QString newText = text();
        newText.replace(m_dragStart, m_dragEnd - m_dragStart, newNum);
        setText(newText);
        setCursorPosition(m_dragStart + newNum.length());
        // Keep the new number selected for visual feedback
        setSelection(m_dragStart, newNum.length());

        // Live evaluate
        bool ok;
        double evalResult = MathEngine::evalSimple(newText, ok);
        if (ok && std::isfinite(evalResult)) {
            showFloatingResult(QString::number(evalResult, 'g', 10));
        }
        else {
            showFloatingResult("Error");
        }

        m_lastGlobalPos = event->globalPos();
        event->accept();
        return;
    }
    QLineEdit::mouseMoveEvent(event);
}

void DraggableExpressionEdit::mouseReleaseEvent(QMouseEvent* event) {
    if (m_dragging) {
        m_dragging = false;
        setCursor(Qt::IBeamCursor);
        deselect(); // remove highlight
        hideFloatingResult();
        event->accept();
        return;
    }
    QLineEdit::mouseReleaseEvent(event);
}