#pragma once
#include <QString>
#include <QVector>
#include <QMap>

struct ShapeInfo {
    QString displayName;      // e.g., "Cube"
    QString internalName;     // e.g., "cube"
    QStringList paramNames;   // e.g., {"s"}
    QMap<QString, double> defaultParams;
    bool defaultRotate = true;
};

inline const QVector<ShapeInfo> ALL_SHAPES_INFO = {
    {"Cube", "cube", {"s"}, {{"s", 1.0}}, true},
    {"Sphere", "sphere", {"r"}, {{"r", 1.0}}, false},
    {"Cylinder", "cylinder", {"r","h"}, {{"r", 0.5}, {"h", 1}}, false}
};