#pragma once

#include <QFrame>
#include <QObject>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include "../constants/Theme.h"

class FocusAnchor : public QFrame {
    Q_OBJECT
public:
    explicit FocusAnchor(QWidget* parent = nullptr) : QFrame(parent) {
        setFixedSize(140, 36);
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::PointingHandCursor);
        setStyleSheet("background:transparent;");

        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(2);

        m_title = new QLabel("FOCUS GLOW (WIP)");
        m_title->setFont(Theme::monoFont(7));
        m_title->setStyleSheet("color:#5a6b5f; letter-spacing:2px;");
        m_title->setAlignment(Qt::AlignCenter);

        m_bar = new QWidget;
        m_bar->setFixedHeight(3);
        m_bar->setStyleSheet("background:#2a3a2f; border-radius:2px;"); // dim when inactive

        m_layout->addWidget(m_title);
        m_layout->addWidget(m_bar);
    }

    // Call this to reflect active/inactive state visually
    void setActive(bool active) {
        m_active = active;
        if (active) {
            m_title->setStyleSheet("color:#00e87a; letter-spacing:2px;");
            m_bar->setStyleSheet("background:#00e87a; border-radius:2px;");
        }
        else {
            m_title->setStyleSheet("color:#5a6b5f; letter-spacing:2px;");
            m_bar->setStyleSheet("background:#2a3a2f; border-radius:2px;");
        }
    }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            emit clicked();
            e->accept();
        }
        else {
            QFrame::mousePressEvent(e);
        }
    }

    void enterEvent(QEnterEvent*) override {
        if (!m_active)
            m_bar->setStyleSheet("background:#3a5a40; border-radius:2px;"); // hover hint
    }

    void leaveEvent(QEvent*) override {
        if (!m_active)
            m_bar->setStyleSheet("background:#2a3a2f; border-radius:2px;");
    }

private:
    QVBoxLayout* m_layout = nullptr;
    QLabel* m_title = nullptr;
    QWidget* m_bar = nullptr;
    bool         m_active = false;
};