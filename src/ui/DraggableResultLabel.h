#pragma once
#include <QLabel>
#include <QString>
#include <QPointer>
#include <QTimer>

class DraggableResultLabel : public QLabel {
    Q_OBJECT
public:
    explicit DraggableResultLabel(const QString& displayText, const QString& plainNumber,
        const QString& originalExpr, QWidget* parent = nullptr);
    void updateResult(const QString& newDisplayText, const QString& newPlainNumber,
        const QString& newOriginalExpr);

signals:
    void resultChanged(const QString& newExpr, const QString& newResult);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void rebuildExpression(double newVal);
    void showFloatingResult(const QString& result);
    void hideFloatingResult();

    QString m_originalExpr;      // the expression that produced this result
    QString m_currentDisplay;    // displayed text (may include units or formula)
    QString m_originalNumber;    // the exact number string from the expression (to be replaced)
    double m_originalValue = 0.0;
    bool m_dragging = false;
    double m_currentDelta = 0.0;
    QPoint m_lastGlobalPos;
    QPointer<QLabel> m_floatingLabel;
};