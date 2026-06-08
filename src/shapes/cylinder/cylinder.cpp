#include "cylinder.h"
#include <QOpenGLFunctions>
#include <QString>
#include <QMap>
#include <cmath>

void Cylinder::setParameters(const QMap<QString, double>& params)
{
    m_radius = params.value("r");
    m_height = params.value("h");
    segments = 1024;

}

void Cylinder::draw() {
    float halfH = m_height / 2.0f;
    float angleStep = 2 * M_PI / segments;

    // Vertical lines (sides) – draw in dark gray (optional, comment out to remove)
    glColor3f(0.3f, 0.3f, 0.3f);
    glBegin(GL_LINES);
    for (int i = 0; i < segments; ++i) {
        float angle = i * angleStep;
        float x = m_radius * cos(angle);
        float z = m_radius * sin(angle);
        glVertex3f(x, -halfH, z);
        glVertex3f(x, halfH, z);
    }
    glEnd();

    // Top and bottom circles – draw in white (or shape color)
    glColor3f(1.0f, 1.0f, 1.0f);
    // Top circle
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * angleStep;
        float x = m_radius * cos(angle);
        float z = m_radius * sin(angle);
        glVertex3f(x, halfH, z);
    }
    glEnd();
    // Bottom circle
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < segments; ++i) {
        float angle = i * angleStep;
        float x = m_radius * cos(angle);
        float z = m_radius * sin(angle);
        glVertex3f(x, -halfH, z);
    }
    glEnd();
}
