#pragma once
#include "../MeshShape.h"

class Circle : public MeshShape {
public:
    void setParameters(const QMap<QString, double>& params) override;
    void draw() override;
    void drawShader(QOpenGLShaderProgram& shader) override;
    bool isCurved() const override { return true; }
    ~Circle() override;

protected:
    void buildMesh() override;
    bool  hasNormals() const override { return false; }  // position-only mesh

private:
    float m_radius = 1.0f;
    int   m_segments = 512;   // number of sides
};