#pragma once
#include "../RenderShape.h"

class Sphere : public RenderShape {
public:
    void draw() override;
    void setParameters(const QMap<QString, double>& params) override;
private:
    float m_radius = 1.0f;
    int m_slices = 36, m_stacks = 36;
};