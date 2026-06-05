#include "Animations.h"
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>

void Animations::fadeIn(QWidget* widget, int durationMs) {
    if (!widget) return;
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    QPropertyAnimation* anim = new QPropertyAnimation(effect, "opacity");
    anim->setDuration(durationMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void Animations::flash(QWidget* widget, const QString& flashColor, int durationMs) {
    if (!widget) return;
    QString originalStyle = widget->styleSheet();
    widget->setStyleSheet(QString("background-color: %1;").arg(flashColor));
    QTimer::singleShot(durationMs, widget, [widget, originalStyle]() {
        widget->setStyleSheet(originalStyle);
        });
}