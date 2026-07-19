#pragma once
#include <QMap>
#include <QOpenGLShaderProgram>
#include <QString>

class RenderShape {
public:
    virtual ~RenderShape() = default;
    virtual void draw() = 0;
    virtual void drawShader(QOpenGLShaderProgram& shader) {} // default no-op
    virtual void setParameters(const QMap<QString, double>& params) = 0;

    // Curved surfaces (sphere, cylinder, circle) tessellate into a grid whose
    // edges look like ugly criss-cross lines under the wireframe overlay, so the
    // renderer skips that overlay for them. Flat-faced solids (cube, cuboid)
    // return false and keep their edge overlay, where the lines are real edges.
    virtual bool isCurved() const { return false; }
};