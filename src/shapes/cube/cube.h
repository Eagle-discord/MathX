#pragma once
#include "../RenderShape.h"

class Cube : public RenderShape {
public:
    void draw() override;
    void setParameters(const QMap<QString, double>& params) override;
private:
    float m_size = 1.0f;
};