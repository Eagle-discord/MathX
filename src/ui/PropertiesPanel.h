// src/ui/PropertiesPanel.h
#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include "../constants/Theme.h"

class PropertiesPanel : public QWidget {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget* parent = nullptr) : QWidget(parent) {
        setStyleSheet("background: rgba(0,0,0,160); border-radius: 8px;");

        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(0, 12, 0, 12);
        m_layout->setSpacing(0);


        m_layout->addStretch();
    }

    QVBoxLayout* contentLayout() { return m_layout; }

private:
    QVBoxLayout* m_layout = nullptr;
};