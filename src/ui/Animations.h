#pragma once
#include <QWidget>
#include <QString>

class Animations {
public:
    static void fadeIn(QWidget* widget, int durationMs = 200);
    static void flash(QWidget* widget, const QString& flashColor = "#ffffff", int durationMs = 100);
};