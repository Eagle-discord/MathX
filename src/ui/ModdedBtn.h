#pragma once

#include <QPushButton>

class ModdedBtn : public QPushButton
{
    Q_OBJECT

public:
    using QPushButton::QPushButton;

signals:
    void hoverEnter();
    void hoverLeave();

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
};