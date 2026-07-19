#include "ShapeRegistry.h"
#include "ShapeDefs.h"

std::unique_ptr<RenderShape> ShapeRegistry::create(const QString& type) {
    const ShapeDef* d = def(type);
    return (d && d->factory) ? d->factory() : nullptr;
}

const ShapeDef* ShapeRegistry::def(const QString& type) {
    return findShapeDef(type);
}

bool ShapeRegistry::contains(const QString& type) {
    return findShapeDef(type) != nullptr;
}

QStringList ShapeRegistry::all() {
    QStringList list;
    for (const auto& d : ALL_SHAPE_DEFS())
        list << d.internalName;
    return list;
}

QStringList ShapeRegistry::allWithViewer() {
    QStringList list;
    for (const auto& d : ALL_SHAPE_DEFS())
        if (d.factory) list << d.internalName;
    return list;
}