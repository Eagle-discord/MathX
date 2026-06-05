#include "HistoryDock.h"
#include <QVBoxLayout>
#include <QMenu>
#include <QContextMenuEvent>
#include <QApplication>
#include <QClipboard>

HistoryDock::HistoryDock(QWidget* parent) : QDockWidget("History", parent) {
    setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list);
    container->setLayout(layout);
    setWidget(container);

    connect(m_list, &QListWidget::itemDoubleClicked, this, &HistoryDock::onItemDoubleClicked);
    connect(m_list, &QListWidget::customContextMenuRequested, this, &HistoryDock::onContextMenu);
}

void HistoryDock::addEntry(const QString& expression, const QString& result) {
    QString display = expression + " = " + result;
    QListWidgetItem* item = new QListWidgetItem(display, m_list);
    item->setData(Qt::UserRole, expression);   // store the original expression
    m_list->addItem(item);
    m_list->scrollToBottom();
}

void HistoryDock::clearHistory() {
    m_list->clear();
}

void HistoryDock::onItemDoubleClicked(QListWidgetItem* item) {
    QString expr = item->data(Qt::UserRole).toString();
    emit expressionSelected(expr);
}

void HistoryDock::onContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_list->itemAt(pos);
    if (!item) return;

    QMenu menu;
    QAction* copyExpr = menu.addAction("Copy Expression");
    QAction* copyResult = menu.addAction("Copy Result");
    QAction* deleteAction = menu.addAction("Delete");
    QAction* clearAll = menu.addAction("Clear All");

    QAction* chosen = menu.exec(m_list->viewport()->mapToGlobal(pos));
    if (chosen == copyExpr) {
        QString expr = item->data(Qt::UserRole).toString();
        QApplication::clipboard()->setText(expr);
    }
    else if (chosen == copyResult) {
        QString fullText = item->text();
        // Extract result after " = "
        int idx = fullText.indexOf(" = ");
        if (idx != -1) {
            QString result = fullText.mid(idx + 3);
            QApplication::clipboard()->setText(result);
        }
    }
    else if (chosen == deleteAction) {
        delete item;
    }
    else if (chosen == clearAll) {
        m_list->clear();
    }
}