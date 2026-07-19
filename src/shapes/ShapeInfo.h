#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <QStringList>
#include "ShapeDefs.h"

// DO NOT EDIT — derived from ALL_SHAPE_DEFS() in ShapeDefs.h
// Add or modify shapes there; this updates automatically.

struct ShapeInfo {
    QString                displayName;
    QString                internalName;
    QStringList            paramNames;
    QMap<QString, double>  defaultParams;
    bool                   defaultRotate;
};

inline QVector<ShapeInfo> buildViewerShapeList() {
    QVector<ShapeInfo> result;
    for (const auto& def : ALL_SHAPE_DEFS()) {
        if (!def.factory) continue; // skip shapes with no viewer
        ShapeInfo info;
        info.displayName = def.displayName;
        info.internalName = def.internalName;
        info.defaultRotate = def.defaultRotate;
        info.paramNames = def.paramNames();
        info.defaultParams = def.defaultParams();
        result.push_back(info);
    }
    return result;
}

// Cached — built once on first call
inline const QVector<ShapeInfo>& ALL_SHAPES_INFO() {
    static const QVector<ShapeInfo> list = buildViewerShapeList();
    return list;
}