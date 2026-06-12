#pragma once
#include <QLabel>
#include <QTimer>

class OutputWidget : public QLabel {
    Q_OBJECT
public:
    explicit OutputWidget(const QString& displayText,
        const QString& copyText,
        const QString& originalExpr,
        const QString& formula,
        const QString& type,
        const QString& color,
        const QString& extraStyle = "padding-left:22px;",
        QWidget* parent = nullptr);

signals:
    void expressionChanged(const QString& newExpr, const QString& newResult); // optional

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void copyToClipboard();
    void resetCopyState();

    QString m_displayText;
    QString m_copyText;
    QString m_originalExpr;
    QString m_formula;
    QString m_type;
    QString m_color;
    bool m_copied = false;
    QTimer m_copyResetTimer;
    bool m_formulaVisible = false;
};