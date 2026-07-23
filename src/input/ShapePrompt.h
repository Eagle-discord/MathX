#pragma once
#include <QString>
#include <QStringList>
#include <QMap>
#include "../shapes/ShapeDef.h"
struct ShapePrompt {
    QString command;
    QStringList params;
    QStringList optional;
    QMap<QString, QString> collected;
    int currentIndex = 0;

    bool isActive() const { return currentIndex < params.size(); }
    QString nextParam() const { return params.value(currentIndex); }
};