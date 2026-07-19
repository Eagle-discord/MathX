#include "circle.h"
#include <QOpenGLFunctions>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Circle::setParameters(const QMap<QString, double>& params) {
    float newRadius = params.value("r", 1.0f);
    if (newRadius <= 0.0f) newRadius = 1.0f;

    // FIX: the original read `static_cast<int>(m_segments)` — i.e. it read
    // the shape's *own current* segment count instead of looking it up in
    // `params`. That made `newSegs != m_segments` always false, so segment
    // resolution could never actually change at runtime. Now reads from
    // params, falling back to the current value if not supplied.
    int newSegs = static_cast<int>(params.value("segments", m_segments));
    if (newSegs < 3) newSegs = 3;   // at least a triangle

    if (newRadius != m_radius || newSegs != m_segments) {
        m_radius = newRadius;
        m_segments = newSegs;
        m_meshDirty = true;
    }
}

void Circle::buildMesh() {
    m_vertices.clear();
    m_indices.clear();

    // Centre vertex
    m_vertices.push_back(0.0f);
    m_vertices.push_back(0.0f);
    m_vertices.push_back(0.0f);

    // Circumference vertices
    for (int i = 0; i < m_segments; ++i) {
        float theta = 2.0f * M_PI * i / m_segments;
        m_vertices.push_back(m_radius * std::cos(theta));
        m_vertices.push_back(m_radius * std::sin(theta));
        m_vertices.push_back(0.0f);
    }

    // Triangle fan indices
    for (int i = 0; i < m_segments; ++i) {
        int next = (i + 1) % m_segments;
        m_indices.push_back(0);
        m_indices.push_back(static_cast<GLuint>(i + 1));
        m_indices.push_back(static_cast<GLuint>(next + 1));
    }

    m_meshDirty = false;
}

void Circle::drawShader(QOpenGLShaderProgram& shader) {
    ensureUploaded(shader);
    bindForDraw(shader);

    // Filled disk
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);


    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    unbindAfterDraw(shader);
}

// Immediate-mode: filled circle (triangle fan)
void Circle::draw() {
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(0.0f, 0.0f, 0.0f);
    for (int i = 0; i <= m_segments; ++i) {
        float theta = 2.0f * M_PI * i / m_segments;
        float x = m_radius * std::cos(theta);
        float y = m_radius * std::sin(theta);
        glVertex3f(x, y, 0.0f);
    }
    glEnd();
}

Circle::~Circle() = default;