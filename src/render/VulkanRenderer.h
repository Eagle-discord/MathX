#pragma once
#include "Renderer.h"

class VulkanRenderer : public Renderer {
public:
    bool initialize(QWidget* parent) override { return false; }
    void setShape(const QString&, const QMap<QString, double>&) override {}
    void setDarkTheme(bool) override {}
    void setRotationEnabled(bool) override {}
    void setMouseCtrl(bool) override {};
    QWidget* widget() override { return nullptr; }
};