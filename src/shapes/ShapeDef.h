#pragma once
#include <QString>
#include <QStringList>
#include <QList>
#include <QMap>

// -- Shape ---------------------------------------------------------------------
// A single shape's parsing contract. `substitutes` maps a required parameter to
// the set of parameters that can stand in for it: {"r", {"d"}} means an
// expression supplying `d` satisfies the requirement for `r`.
//
// Substitution answers *satisfiability only*. The arithmetic that turns d into
// r (r = d/2) lives in the card factory, because it varies per shape and this
// table should stay a table.
struct Shape {
    QString     canonical;       // internal name (e.g. "tri", "cube")
    QStringList aliases;         // alternative names (e.g. "triangle")
    QStringList requiredParams;  // must be provided, or satisfied via substitutes
    QStringList optionalParams;  // may be provided; never required
    QMap<QString, QStringList> substitutes;  // required -> params that satisfy it
};

// Central list of all shape definitions
inline const QList<Shape> ALL_SHAPES = {
    // 2D shapes
    { "circ",      {"circle"},                                {"r"},             {"d"},  {{"r", {"d"}}} },
    { "ellipse",   {"ellpse"},                                {"a","b"},         {},     {} },
    { "tri",       {"triangle"},                              {"a","b"},         {"c"},  {} },
    { "righttri",  {"rtri","righttriangle"},                  {"a","b"},         {},     {} },
    { "rect",      {"rectangle"},                             {"w","h"},         {},     {} },
    { "sqre",      {"square"},                                {"s"},             {},     {} },
    { "parallel",  {"parallelogram","pgram","parallelgram"},  {"b","h","s"},     {},     {} },
    { "trap",      {"trapezoid","trapezium"},                 {"a","b","h"},     {},     {} },
    { "rhombus",   {"rhomb"},                                 {"d1","d2"},       {},     {} },
    { "regpoly",   {"polygon","regular polygon"},             {"n","s"},         {},     {} },
    { "sector",    {"sect"},                                  {"r","angle"},     {},     {} },
    { "annulus",   {"ring"},                                  {"major","minor"}, {},     {} },

    // 3D shapes
    { "sphr",      {"sphere"},                                {"r"},             {"d"},  {{"r", {"d"}}} },
    { "hemi",      {"hemisphere"},                            {"r"},             {"d"},  {{"r", {"d"}}} },
    { "cyld",      {"cylinder"},                              {"r","h"},         {"d"},  {{"r", {"d"}}} },
    { "hollcyl",   {"hollowcylinder","hollowcyl"},            {"R","r","h"},     {"ro"}, {{"R", {"ro"}}} },
    { "cone",      {},                                        {"r","h"},         {},     {} },
    { "frustum",   {"truncatedcone"},                         {"R","r","h"},     {"ro"}, {{"R", {"ro"}}} },
    { "cuboid",    {"box"},                                   {"l","w","h"},     {},     {} },
    { "cube",      {},                                        {"s"},             {},     {} },
    { "tetra",     {"tetrahedron"},                           {"s"},             {},     {} },
    { "octa",      {"octahedron"},                            {"s"},             {},     {} },
    { "icosa",     {"icosahedron"},                           {"s"},             {},     {} },
    { "dodeca",    {"dodecahedron"},                          {"s"},             {},     {} },
    { "prism",     {},                                        {"n","s","h"},     {},     {} },
    { "pyramid",   {"sqpyramid"},                             {"b","h"},         {},     {} },
    { "torus",     {"donut"},                                 {"major","minor"}, {},     {} },
    { "ellipsoid", {"ellpsoid"},                              {"a","b","c"},     {},     {} },
    { "capsule",   {"pill"},                                  {"r","h"},         {},     {} },
};
