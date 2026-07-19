#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QMap>
#include <QVector>
#include <QPair>

struct RefItem {
    QString text;
    QString note;
};
struct RefSection {
    QString label;
    QVector<RefItem> items;
};

class SidebarPanel : public QWidget {
    Q_OBJECT
public:
    explicit SidebarPanel(QWidget* parent = nullptr);
    void renderMode(const QString& mode);



signals:
    void itemClicked(const QString& expr);
    void itemDoubleClicked(const QString& expr);

private:
    QWidget*     m_container = nullptr;
    QVBoxLayout* m_layout    = nullptr;
    QScrollArea* m_scroll    = nullptr;
    QMap<QString, QList<QWidget*>> m_sectionWidgets; // mode -> all widgets for that mode
    QString m_currentMode;
    QMap<QString, QVector<RefSection>> m_refs;
    void buildRefs();

    void buildMode(const QString& mode);
};
