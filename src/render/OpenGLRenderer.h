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
    void setShapeColor(float r, float g, float b);
    void setBackgroundColor(float r, float g, float b);
    // Renderer interface
    bool initialize(QWidget* parent) override;
    void setShape(const QString& type, const QMap<QString, double>& params) override;
    void setDarkTheme(bool dark) override;
    void setRotationEnabled(bool enabled) override;
    QWidget* widget() override { return this; }
    void setHorizontalOffset(int offsetPixels);
    void setMouseCtrl(bool enabled) override;
protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void updateRotation();
    void drawSphere(float radius, int slices, int stacks);
    void drawCube(float size);
    void drawCylinder(float radius, float height, int segments);
    bool m_mousePressed = false;
    bool m_mouseCtrlEnabled = false;
    QPoint m_lastMousePos;
    float m_camDistance = 5.0f; // optional zoom
    std::unique_ptr<RenderShape> m_currentShape;
    QString m_shapeType;
    QMap<QString, double> m_params;
    float m_xRot = 0.0f;
    float m_yRot = 0.0f;
    float m_colorR = 1.0f, m_colorG = 1.0f, m_colorB = 1.0f;
    float m_bgR = 0.0f, m_bgG = 0.0f, m_bgB = 0.0f;
    bool m_rotating = true;
    bool m_dark = true;
    QTimer m_rotationTimer;
    int m_horizontalOffset = 0;
};