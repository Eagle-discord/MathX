#include "ModdedBtn.h"

void ModdedBtn::enterEvent(QEnterEvent* event)
{
    QPushButton::enterEvent(event);
    emit hoverEnter();
}

void ModdedBtn::leaveEvent(QEvent* event)
{
    
    QPushButton::leaveEvent(event);
    emit hoverLeave();
}