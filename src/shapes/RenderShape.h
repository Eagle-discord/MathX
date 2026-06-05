#pragma once
#include <QMap>

class RenderShape {
public:
    virtual ~RenderShape() = default;
    virtual void draw() = 0;
    virtual void setParameters(const QMap<QString, double>& params) = 0;
};