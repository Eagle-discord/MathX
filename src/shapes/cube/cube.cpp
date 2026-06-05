#include "Cube.h"
#include <QOpenGLFunctions>  // for glVertex3f, etc.
// Note: OpenGL includes are already handled by the renderer's context.

void Cube::setParameters(const QMap<QString, double>& params) {
    m_size = params.value("s", 1.0f);
}

void Cube::draw() {
    float half = m_size / 2.0f;
    glBegin(GL_QUADS);
    // front
    glVertex3f(-half, -half, half);
    glVertex3f(half, -half, half);
    glVertex3f(half, half, half);
    glVertex3f(-half, half, half);
    // back
    glVertex3f(-half, -half, -half);
    glVertex3f(half, -half, -half);
    glVertex3f(half, half, -half);
    glVertex3f(-half, half, -half);
    // left
    glVertex3f(-half, -half, -half);
    glVertex3f(-half, -half, half);
    glVertex3f(-half, half, half);
    glVertex3f(-half, half, -half);
    // right
    glVertex3f(half, -half, -half);
    glVertex3f(half, -half, half);
    glVertex3f(half, half, half);
    glVertex3f(half, half, -half);
    // top
    glVertex3f(-half, half, -half);
    glVertex3f(half, half, -half);
    glVertex3f(half, half, half);
    glVertex3f(-half, half, half);
    // bottom
    glVertex3f(-half, -half, -half);
    glVertex3f(half, -half, -half);
    glVertex3f(half, -half, half);
    glVertex3f(-half, -half, half);
    glEnd();
}