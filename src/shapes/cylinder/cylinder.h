#pragma once
#include "../MeshShape.h"

class Cylinder : public MeshShape {
public:
    void setParameters(const QMap<QString, double>& params) override;
    void draw() override;
    bool isCurved() const override { return true; }
    ~Cylinder() override;

protected:
    void buildMesh() override;

private:
    float m_radius = 1.0f;
    float m_height = 2.0f;
    int   m_segments = 64;
};