// ShapeRegistry.h
#pragma once
#include <QString>
#include <QMap>
#include <memory>
#include "RenderShape.h"
#include "cube/cube.h"
#include "sphere/sphere.h"
#include "cylinder/cylinder.h"
#include "cuboid/cuboid.h"
#include <functional>
struct ShapeDef;
#include <qstringlist.h>
#include "RenderMode.h"




struct ShapeMeta {
    RenderMode defaultRenderMode;
    std::function<std::unique_ptr<RenderShape>()> factory;
};


class ShapeRegistry {
public:
    static std::unique_ptr<RenderShape> create(const QString& type);
    static const ShapeDef* def(const QString& type);
    static bool                        contains(const QString& type);
    static QStringList                 all();
    static QStringList                 allWithViewer();
};