#pragma once
#include <QString>
#include <QMap>
#include <QWidget>

class Renderer {
public:
    virtual ~Renderer() = default;
    virtual bool initialize(QWidget* parent) = 0;
    virtual void setShape(const QString& type, const QMap<QString, double>& params) = 0;
    virtual void setDarkTheme(bool dark) = 0;
    virtual void setRotationEnabled(bool enabled) = 0;
    virtual void setMouseCtrl(bool enabled) = 0;
    virtual void clearMesh() = 0;
    virtual QWidget* widget() = 0;
};