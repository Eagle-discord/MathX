#pragma once

#include "Renderer.h"
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QTimer>
#include <QMap>
#include <memory>
#include "../shapes/RenderShape.h"


class OpenGLRenderer : public QOpenGLWidget, public Renderer, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit OpenGLRenderer(QWidget* parent = nullptr);
    ~OpenGLRenderer() = default;

    // Renderer interface
    bool initialize(QWidget* parent) override;
    void setShape(const QString& type, const QMap<QString, double>& params) override;
    void setDarkTheme(bool dark) override;
    void setRotationEnabled(bool enabled) override;
    QWidget* widget() override { return this; }
    void setHorizontalOffset(int offsetPixels);
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;


private:
    void updateRotation();
    void drawSphere(float radius, int slices, int stacks);
    void drawCube(float size);
    void drawCylinder(float radius, float height, int segments);
    std::unique_ptr<RenderShape> m_currentShape;
    QString m_shapeType;
    QMap<QString, double> m_params;
    float m_xRot = 0.0f;
    float m_yRot = 0.0f;
    bool m_rotating = true;
    bool m_dark = true;
    QTimer m_rotationTimer;
    int m_horizontalOffset = 0;
};