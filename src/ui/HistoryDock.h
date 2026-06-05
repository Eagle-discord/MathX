#pragma once
#include <QDockWidget>
#include <QListWidget>

class HistoryDock : public QDockWidget {
    Q_OBJECT
public:
    explicit HistoryDock(QWidget* parent = nullptr);
    void addEntry(const QString& expression, const QString& result);
    void clearHistory();

signals:
    void expressionSelected(const QString& expr);

private slots:
    void onItemDoubleClicked(QListWidgetItem* item);
    void onContextMenu(const QPoint& pos);

private:
    QListWidget* m_list;
};