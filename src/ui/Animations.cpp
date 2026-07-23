#include "Animations.h"
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QPointer>

void Animations::fadeIn(QWidget* widget, int durationMs) {
    if (!widget) return;
    QGraphicsOpacityEffect* effect = new QGraphicsOpacityEffect(widget);
    widget->setGraphicsEffect(effect);
    QPropertyAnimation* anim = new QPropertyAnimation(effect, "opacity", widget);
    anim->setDuration(durationMs);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);

    // Drop the effect once the fade is done. A QGraphicsOpacityEffect forces its
    // widget onto an offscreen composite path for as long as it's attached, so
    // leaving it in place made every finished line a permanent render tax -
    // noticeable by the third line and worse from there.
    QPointer<QWidget> safe(widget);
    QObject::connect(anim, &QPropertyAnimation::finished, widget, [safe]() {
        if (safe) safe->setGraphicsEffect(nullptr);   // deletes the effect
        });

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