#pragma once
#include <QString>
#include <QVector>
#include <QMap>
#include <functional>
#include <memory>
#include "RenderShape.h"
#include "Shapes2D.h"
#include "Shapes3D.h"
#include "cube/cube.h"
#include "sphere/sphere.h"
#include "cylinder/cylinder.h"
#include "cuboid/cuboid.h"
#include "ShapeRegistry.h"
#include "RenderMode.h"
#include "../shapes/ShapeDefs.h"
#include "circle/circle.h"

// Forward declare to avoid circular includes
class GeoCard;
//
// enum class RenderMode;

// -- Param definition ----------------------------------------------------------
struct ParamDef {
    QString name;
    double  min;
    double  max;
    double  defaultVal;
};

// -- Shape categories ----------------------------------------------------------
enum class ShapeCategory { Shape2D, Shape3D };

// -- Central shape definition --------------------------------------------------
struct ShapeDef {
    QString        displayName;
    QString        internalName;
    ShapeCategory  category;
    QVector<ParamDef> params;

    // Viewer (null if shape has no 3D viewer implementation yet)
    RenderMode     renderMode = RenderMode::Wireframe;
    bool           defaultRotate = true;
    std::function<std::unique_ptr<RenderShape>()> factory; // null = no viewer

    // GeoCard factory � always present
    std::function<GeoCard* (const QMap<QString, double>&, QWidget*)> cardFactory;

    // -- Helpers ---------------------------------------------------------------
    QMap<QString, double> defaultParams() const {
        QMap<QString, double> m;
        for (const auto& p : params) m[p.name] = p.defaultVal;
        return m;
    }

    QStringList paramNames() const {
        QStringList l;
        for (const auto& p : params) l << p.name;
        return l;
    }

    const ParamDef* findParam(const QString& name) const {
        for (const auto& p : params)
            if (p.name == name) return &p;
        return nullptr;
    }
};

// -- Registry ------------------------------------------------------------------
inline const QVector<ShapeDef>& ALL_SHAPE_DEFS() {
    static const QVector<ShapeDef> defs = {

        // ======= 3D ===========================================================

        {
            "Sphere", "sphere", ShapeCategory::Shape3D,
            {{"r", 0.1, 10.0, 1.0}},
            RenderMode::Glass, false,
            [] { return std::make_unique<Sphere>(); },
            [](const QMap<QString,double>& p, QWidget* w) { return new SphereCard(p, w); }
        },
        {
            "Hemisphere", "hemisphere", ShapeCategory::Shape3D,
            {{"r", 0.1, 10.0, 1.0}},
            RenderMode::Glass, false,
            nullptr, // no viewer yet
            [](const QMap<QString,double>& p, QWidget* w) { return new HemiSphereCard(p, w); }
        },
        {
            "Cylinder", "cylinder", ShapeCategory::Shape3D,
            {{"r", 0.1, 10.0, 0.5}, {"h", 0.1, 10.0, 1.0}},
            RenderMode::Glass, false,
            [] { return std::make_unique<Cylinder>(); },
            [](const QMap<QString,double>& p, QWidget* w) { return new CylinderCard(p, w); }
        },
        {
            "Hollow Cylinder", "hollowcyl", ShapeCategory::Shape3D,
            {{"R", 0.1, 10.0, 2.0}, {"r", 0.1, 10.0, 1.0}, {"h", 0.1, 10.0, 3.0}},
            RenderMode::Wireframe, false,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new HollowCylCard(p, w); }
        },
        {
            "Cone", "cone", ShapeCategory::Shape3D,
            {{"r", 0.1, 10.0, 1.0}, {"h", 0.1, 10.0, 2.0}},
            RenderMode::Wireframe, true,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new ConeCard(p, w); }
        },
        {
            "Frustum", "frustum", ShapeCategory::Shape3D,
            {{"R", 0.1, 10.0, 2.0}, {"r", 0.1, 10.0, 1.0}, {"h", 0.1, 10.0, 3.0}},
            RenderMode::Wireframe, true,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new FrustumCard(p, w); }
        },
        {
            "Cube", "cube", ShapeCategory::Shape3D,
            {{"s", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, true,
            [] { return std::make_unique<Cube>(); },
            [](const QMap<QString,double>& p, QWidget* w) { return new CubeCard(p, w); }
        },
        {
            "Cuboid", "cuboid", ShapeCategory::Shape3D,
            {{"w", 0.1, 10.0, 1.0}, {"h", 0.1, 10.0, 2.0}, {"l", 0.1, 10.0, 3.0}},
            RenderMode::Wireframe, true,
            [] { return std::make_unique<Cuboid>(); },
            [](const QMap<QString,double>& p, QWidget* w) { return new CuboidCard(p, w); }
        },
        {
            "Tetrahedron", "tetra", ShapeCategory::Shape3D,
            {{"s", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, true,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new TetraCard(p, w); }
        },
        {
            "Octahedron", "octa", ShapeCategory::Shape3D,
            {{"s", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, true,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new OctaCard(p, w); }
        },
        {
            "Icosahedron", "icosa", ShapeCategory::Shape3D,
            {{"s", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, true,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new IcosaCard(p, w); }
        },
        {
            "Dodecahedron", "dodeca", ShapeCategory::Shape3D,
            {{"s", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, true,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new DodecaCard(p, w); }
        },
        {
            "Prism", "prism", ShapeCategory::Shape3D,
            {{"n", 3.0, 12.0, 6.0}, {"s", 0.1, 10.0, 1.0}, {"h", 0.1, 10.0, 2.0}},
            RenderMode::Wireframe, true,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new PrismCard(p, w); }
        },
        {
            "Pyramid", "pyramid", ShapeCategory::Shape3D,
            {{"b", 0.1, 10.0, 2.0}, {"h", 0.1, 10.0, 3.0}},
            RenderMode::Wireframe, true,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new PyramidCard(p, w); }
        },
        {
            "Torus", "torus", ShapeCategory::Shape3D,
            {{"R", 0.1, 10.0, 2.0}, {"r", 0.1, 5.0, 0.5}},
            RenderMode::Glass, false,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new TorusCard(p, w); }
        },
        {
            "Ellipsoid", "ellipsoid", ShapeCategory::Shape3D,
            {{"a", 0.1, 10.0, 3.0}, {"b", 0.1, 10.0, 2.0}, {"c", 0.1, 10.0, 1.0}},
            RenderMode::Glass, false,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new EllipsoidCard(p, w); }
        },
        {
            "Capsule", "capsule", ShapeCategory::Shape3D,
            {{"r", 0.1, 10.0, 1.0}, {"h", 0.1, 10.0, 2.0}},
            RenderMode::Glass, false,
            nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new CapsuleCard(p, w); }
        },

        // ======= 2D ===========================================================

        {
            "Circle", "circle", ShapeCategory::Shape2D,
            {{"r", 0.1, 10.0, 1.0}},
            RenderMode::Glass, false,
            [] { return std::make_unique<Circle>(); },
            [](const QMap<QString,double>& p, QWidget* w) { return new CircleCard(p, w); }
        },
        {
            "Ellipse", "ellipse", ShapeCategory::Shape2D,
            {{"a", 0.1, 10.0, 2.0}, {"b", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new EllipseCard(p, w); }
        },
        {
            "Triangle", "triangle", ShapeCategory::Shape2D,
            {{"a", 0.1, 10.0, 3.0}, {"b", 0.1, 10.0, 4.0}, {"c", 0.1, 10.0, 5.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new TriangleCard(p, w); }
        },
        {
            "Right Triangle", "righttri", ShapeCategory::Shape2D,
            {{"a", 0.1, 10.0, 3.0}, {"b", 0.1, 10.0, 4.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new RightTriCard(p, w); }
        },
        {
            "Rectangle", "rect", ShapeCategory::Shape2D,
            {{"w", 0.1, 10.0, 2.0}, {"h", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new RectCard(p, w); }
        },
        {
            "Square", "square", ShapeCategory::Shape2D,
            {{"s", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new SquareCard(p, w); }
        },
        {
            "Parallelogram", "parallel", ShapeCategory::Shape2D,
            {{"b", 0.1, 10.0, 2.0}, {"h", 0.1, 10.0, 1.0}, {"s", 0.1, 10.0, 2.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new ParallelCard(p, w); }
        },
        {
            "Trapezoid", "trapezoid", ShapeCategory::Shape2D,
            {{"a", 0.1, 10.0, 3.0}, {"b", 0.1, 10.0, 5.0}, {"h", 0.1, 10.0, 2.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new TrapezoidCard(p, w); }
        },
        {
            "Rhombus", "rhombus", ShapeCategory::Shape2D,
            {{"d1", 0.1, 10.0, 4.0}, {"d2", 0.1, 10.0, 3.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new RhombusCard(p, w); }
        },
        {
            "Regular Polygon", "regpoly", ShapeCategory::Shape2D,
            {{"n", 3.0, 12.0, 6.0}, {"s", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new RegPolyCard(p, w); }
        },
        {
            "Sector", "sector", ShapeCategory::Shape2D,
            {{"r", 0.1, 10.0, 1.0}, {"deg", 1.0, 360.0, 90.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new SectorCard(p, w); }
        },
        {
            "Annulus", "annulus", ShapeCategory::Shape2D,
            {{"R", 0.1, 10.0, 2.0}, {"r", 0.1, 10.0, 1.0}},
            RenderMode::Wireframe, false, nullptr,
            [](const QMap<QString,double>& p, QWidget* w) { return new AnnulusCard(p, w); }
        },
    };
    return defs;
}


// -- Lookup helpers ------------------------------------------------------------
inline const ShapeDef* findShapeDef(const QString& internalName) {
    for (const auto& def : ALL_SHAPE_DEFS())
        if (def.internalName == internalName) return &def;
    return nullptr;
}
inline GeoCard* createPropsCard(const QString& internalName,
    const QMap<QString, double>& params,
    QWidget* parent) {
    const ShapeDef* def = findShapeDef(internalName);
    if (!def || !def->cardFactory) return nullptr;
    // Create via cardFactory � the GeoCard Mode is set by passing
    // a special sentinel in params that GeoCard checks
    // OR � simpler � just call cardFactory and hide sliders after
    GeoCard* card = def->cardFactory(params, parent);
    card->setPropertiesOnlyMode(); // strips sliders/title/buttons
    return card;
}
inline QVector<const ShapeDef*> shapeDefs3D() {
    QVector<const ShapeDef*> result;
    for (const auto& def : ALL_SHAPE_DEFS())
        if (def.category == ShapeCategory::Shape3D) result.push_back(&def);
    return result;
}

inline QVector<const ShapeDef*> shapeDefs2D() {
    QVector<const ShapeDef*> result;
    for (const auto& def : ALL_SHAPE_DEFS())
        if (def.category == ShapeCategory::Shape2D) result.push_back(&def);
    return result;
}

inline QVector<const ShapeDef*> shapeDefsWithViewer() {
    QVector<const ShapeDef*> result;
    for (const auto& def : ALL_SHAPE_DEFS())
        if (def.factory) result.push_back(&def);
    return result;
}