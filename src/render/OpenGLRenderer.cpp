#include "OpenGLRenderer.h"
#include <cmath>
#include "../shapes/RenderShape.h"
#include "../shapes/cube/Cube.h"
#include "../shapes/sphere/Sphere.h"
#include <memory>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <shapes/cylinder/cylinder.h>
#include <QMouseEvent>
#include <QWheelEvent>
#include <cmath>

// Helper to avoid GLU dependency – custom perspective
static void setPerspective(float fovY, float aspect, float zNear, float zFar) {
    float fH = tan(fovY / 360.0f * M_PI) * zNear;
    float fW = fH * aspect;
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);
}

OpenGLRenderer::OpenGLRenderer(QWidget* parent)
    : QOpenGLWidget(parent)
{
    setMinimumSize(200, 200);
    connect(&m_rotationTimer, &QTimer::timeout, this, &OpenGLRenderer::updateRotation);
    m_rotationTimer.start(16);
    // Request 4x multisampling for smoother edges
    QSurfaceFormat fmt;
    fmt.setSamples(4);
    setFormat(fmt);

}

bool OpenGLRenderer::initialize(QWidget* /*parent*/) {
    // Already a QWidget, nothing extra needed.
    // This method is called by RenderWidget after creation.
    m_params.clear();
    m_shapeType.clear();
    return true;
}

void OpenGLRenderer::setShape(const QString& type, const QMap<QString, double>& params) {
    m_shapeType = type;
    m_params = params;
 //   update(); // trigger repaint
    if (type == "cube") m_currentShape = std::make_unique<Cube>();
    else if (type == "sphere") m_currentShape = std::make_unique<Sphere>();
    else if (type == "cylinder") m_currentShape = std::make_unique<Cylinder>();
    else {
        m_currentShape.reset();
        return;
    }
    m_currentShape->setParameters(params);
    update();
}

void OpenGLRenderer::setDarkTheme(bool dark) {
    m_dark = dark;
    // Re‑initialize clear color (will be applied in paintGL or initializeGL)
    if (isValid()) {
        makeCurrent();
        glClearColor(m_dark ? 0.1f : 0.9f, m_dark ? 0.1f : 0.9f, m_dark ? 0.1f : 0.9f, 1.0f);
        doneCurrent();
        update();
    }
}
void OpenGLRenderer::setMouseCtrl(bool enabled) {
    m_mouseCtrlEnabled = enabled;
}

void OpenGLRenderer::mousePressEvent(QMouseEvent* event) {
    if ((event->button() == Qt::RightButton || event->button() == Qt::LeftButton) && !m_rotating) {
        m_mousePressed = true;
        m_lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    QOpenGLWidget::mousePressEvent(event);
}

void OpenGLRenderer::mouseMoveEvent(QMouseEvent* event) {
    if (m_mousePressed && m_mouseCtrlEnabled) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_lastMousePos = event->pos();
        // Sensitivity: 0.5 degrees per pixel
        m_xRot += delta.y() * 0.5f;
        m_yRot -= delta.x() * 0.5f;
        update();
        event->accept();
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void OpenGLRenderer::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::RightButton || event->button() == Qt::LeftButton) {
        m_mousePressed = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

// Optional: zoom with mouse wheel
void OpenGLRenderer::wheelEvent(QWheelEvent* event) {
    float delta = -event->angleDelta().y() / 60.0f;
    m_camDistance += delta * 0.25f;
//    if (m_camDistance < 1.0f) m_camDistance = 1.0f;
  //  if (m_camDistance > 15.0f) m_camDistance = 15.0f;
    update();
    event->accept();
}

void OpenGLRenderer::setRotationEnabled(bool enabled) {
    m_rotating = enabled;
}
void OpenGLRenderer::setShapeColor(float r, float g, float b) {
    m_colorR = r; m_colorG = g; m_colorB = b;
    update(); // redraw
}

void OpenGLRenderer::setBackgroundColor(float r, float g, float b) {
    m_bgR = r; m_bgG = g; m_bgB = b;
    if (isValid()) {
        makeCurrent();
        glClearColor(m_bgR, m_bgG, m_bgB, 1.0f);
        doneCurrent();
        update();
    }
}
void OpenGLRenderer::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(m_bgR, m_bgG, m_bgB, 0.0f);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);               // show all faces
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
}

void OpenGLRenderer::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    setPerspective(45.0f, (float)w / (float)h, 0.1f, 100.0f);
    glMatrixMode(GL_MODELVIEW);
}

void OpenGLRenderer::paintGL() {
  /*  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -5.0f);
    glRotatef(m_xRot, 1.0f, 0.0f, 0.0f);
    glRotatef(m_yRot, 0.0f, 1.0f, 0.0f);
    glColor3f(1.0f, 1.0f, 1.0f); // white
    float offsetX = 2.0f * m_horizontalOffset / width();

    
    if (m_shapeType == "cube") {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE); // wireframe mode
        drawCube(1.5f);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL); // restore
    }
    else if (m_shapeType == "sphere") {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        drawSphere(1.0f, 36, 36);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    else if (m_shapeType == "cylinder") {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        drawCylinder(0.8f, 1.5f, 36);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
}*/
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -m_camDistance);
    glRotatef(m_xRot, 1.0f, 0.0f, 0.0f);
    glRotatef(m_yRot, 0.0f, 1.0f, 0.0f);
    glColor3f(m_colorR, m_colorG, m_colorB); // white wireframe
    glLineWidth(3.0f);   // thicker lines (default is 1.0)

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

        if (m_currentShape) {
            m_currentShape->draw();
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}



void OpenGLRenderer::updateRotation() {
    if (m_rotating) {
        m_xRot += 1.0f;
        m_yRot += 0.7f;
        update();
    }
}
void OpenGLRenderer::drawSphere(float radius, int slices, int stacks) {
    for (int i = 0; i <= stacks; ++i) {
        float lat0 = M_PI * (-0.5f + (float)(i - 1) / stacks);
        float lat1 = M_PI * (-0.5f + (float)i / stacks);
        float z0 = radius * sin(lat0);
        float z1 = radius * sin(lat1);
        float r0 = radius * cos(lat0);
        float r1 = radius * cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= slices; ++j) {
            float lng = 2 * M_PI * (float)j / slices;
            float x = cos(lng);
            float y = sin(lng);
            glVertex3f(x * r0, y * r0, z0);
            glVertex3f(x * r1, y * r1, z1);
        }
        glEnd();
    }
}

void OpenGLRenderer::drawCube(float size) {
    float half = size / 2.0f;
    glBegin(GL_QUADS);
    // Front
    glVertex3f(-half, -half, half); glVertex3f(half, -half, half);
    glVertex3f(half, half, half); glVertex3f(-half, half, half);
    // Back
    glVertex3f(-half, -half, -half); glVertex3f(half, -half, -half);
    glVertex3f(half, half, -half); glVertex3f(-half, half, -half);
    // Left
    glVertex3f(-half, -half, -half); glVertex3f(-half, -half, half);
    glVertex3f(-half, half, half); glVertex3f(-half, half, -half);
    // Right
    glVertex3f(half, -half, -half); glVertex3f(half, -half, half);
    glVertex3f(half, half, half); glVertex3f(half, half, -half);
    // Top
    glVertex3f(-half, half, -half); glVertex3f(half, half, -half);
    glVertex3f(half, half, half); glVertex3f(-half, half, half);
    // Bottom
    glVertex3f(-half, -half, -half); glVertex3f(half, -half, -half);
    glVertex3f(half, -half, half); glVertex3f(-half, -half, half);
    glEnd();
}

void OpenGLRenderer::drawCylinder(float radius, float height, int segments) {
    float halfH = height / 2.0f;
    // Sides
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2 * M_PI * (float)i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, -halfH, z);
        glVertex3f(x, halfH, z);
    }
    glEnd();
    // Top cap
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, halfH, 0);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2 * M_PI * (float)i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, halfH, z);
    }
    glEnd();
    // Bottom cap
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0, -halfH, 0);
    for (int i = 0; i <= segments; ++i) {
        float angle = 2 * M_PI * (float)i / segments;
        float x = radius * cos(angle);
        float z = radius * sin(angle);
        glVertex3f(x, -halfH, z);
    }
    glEnd();
}
void OpenGLRenderer::setHorizontalOffset(int offsetPixels) {
    m_horizontalOffset = offsetPixels;
}