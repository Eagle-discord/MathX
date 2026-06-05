#pragma once
#include <QLineEdit>
#include <QPointer>
#include <QLabel>

class DraggableExpressionEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit DraggableExpressionEdit(QWidget* parent = nullptr);

signals:
    void expressionChanged(const QString& expr, double newValue); // optional

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool findNumberAtPos(int pos, int& start, int& end, double& value);
    void updateNumber(int start, int end, double delta);
    void showFloatingResult(const QString& result);
    void hideFloatingResult();

    bool m_dragging = false;
    int m_dragStart = -1, m_dragEnd = -1;
    double m_originalValue = 0.0;
    QPoint m_lastGlobalPos;
    QPointer<QLabel> m_floatingLabel;
    double m_currentDelta = 0.0;
};