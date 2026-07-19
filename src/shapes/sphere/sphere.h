#pragma once
#include "../MeshShape.h"

class Sphere : public MeshShape {
public:
    void setParameters(const QMap<QString, double>& params) override;
    void draw() override;
    bool isCurved() const override { return true; }
    ~Sphere() override;

protected:
    void buildMesh() override;

private:
    float m_radius = 1.0f;
    int   m_stacks = 64;
    int   m_slices = 64;
};