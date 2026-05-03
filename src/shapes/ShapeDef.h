#pragma once
#include <QString>
#include <QStringList>
#include <QList>

struct Shape {
    QString canonical;          // internal name (e.g., "tri", "cube")
    QStringList aliases;        // alternative names (e.g., "triangle", "tetrahedron")
    QStringList requiredParams; // parameters that must be provided (e.g., for triangle: "a","b")
    QStringList optionalParams; // optional parameters (e.g., "c" for triangle's hypotenuse)
};

// Central list of all shape definitions
inline const QList<Shape> ALL_SHAPES = {
    // 2D shapes
    { "circ",      {"circle"},                      {"r"},         {"d"} },
    { "ellipse",   {},                               {"a","b"},     {} },
    { "tri",       {"triangle"},                    {"a","b"},     {"c"} },
    { "righttri",  {"rtri","righttriangle"},        {"a","b"},     {} },
    { "rect",      {"rectangle"},                   {"w","h"},     {} },
    { "sqre",      {"square"},                      {"s"},         {} },
    { "parallel",  {"parallelogram","pgram"},       {"b","h","s"}, {} },
    { "trap",      {"trapezoid","trapezium"},       {"a","b","h"}, {} },
    { "rhombus",   {"rhomb"},                       {"d1","d2"},   {} },
    { "regpoly",   {"polygon"},                     {"n","s"},     {} },
    { "sector",    {},                               {"r","angle"}, {} },
    { "annulus",   {"ring"},                        {"major","minor"},     {} },

    // 3D shapes
    { "sphr",      {"sphere"},                      {"r"},         {} },
    { "hemi",      {"hemisphere"},                  {"r"},         {} },
    { "cyld",      {"cylinder"},                    {"r","h"},     {} },
    { "hollcyl",   {"hollowcylinder","hollowcyl"},  {"R","r","h"}, {} },
    { "cone",      {},                               {"r","h"},     {} },
    { "frustum",   {"truncatedcone"},               {"R","r","h"}, {} },
    { "cuboid",    {"box"},                         {"l","w","h"}, {} },
    { "cube",      {},                               {"s"},         {} },
    { "tetra",     {"tetrahedron"},                 {"s"},         {} },
    { "octa",      {"octahedron"},                  {"s"},         {} },
    { "icosa",     {"icosahedron"},                 {"s"},         {} },
    { "dodeca",    {"dodecahedron"},                {"s"},         {} },
    { "prism",     {},                               {"n","s","h"}, {} },
    { "pyramid",   {"sqpyramid"},                   {"b","h"},     {} },
    { "torus",     {"donut"},                       {"major","minor"},     {} },
    { "ellipsoid", {},                               {"a","b","c"}, {} },
    { "capsule",   {"pill"},                        {"r","h"},     {} },
};