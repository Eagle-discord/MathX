#include "cylinder.h"
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Cylinder::setParameters(const QMap<QString, double>& params) {
    float newR = params.value("r", 1.0f);
    float newH = params.value("h", 2.0f);
    // FIX: guard against non-positive radius/height producing degenerate
    // or inside-out geometry with no warning.
    if (newR <= 0.0f) newR = 1.0f;
    if (newH <= 0.0f) newH = 2.0f;

    if (newR != m_radius || newH != m_height) {
        m_radius = newR;
        m_height = newH;
        m_meshDirty = true;
    }
}

void Cylinder::buildMesh() {
    m_vertices.clear();
    m_indices.clear();

    float halfH = m_height / 2.0f;
    float angleStep = 2.0f * M_PI / m_segments;

    // -- Side vertices: bottom ring then top ring
    for (int i = 0; i <= 1; ++i) {
        float y = (i == 0) ? -halfH : halfH;
        for (int j = 0; j <= m_segments; ++j) {
            float angle = j * angleStep;
            float x = cos(angle), z = sin(angle);
            m_vertices.push_back(x * m_radius);
            m_vertices.push_back(y);
            m_vertices.push_back(z * m_radius);
            m_vertices.push_back(x);
            m_vertices.push_back(0.0f);
            m_vertices.push_back(z);
        }
    }

    int ringSize = m_segments + 1;
    for (int j = 0; j < m_segments; ++j) {
        GLuint b0 = j, b1 = j + 1;
        GLuint t0 = ringSize + j, t1 = ringSize + j + 1;
        m_indices.insert(m_indices.end(), { b0, t0, b1, b1, t0, t1 });
    }

    auto addCap = [&](float y, float ny) {
        // FIX: was `(int)m_vertices.size() / 6` � implicit narrowing from
        // size_t to int. Harmless at these mesh sizes but wrong type;
        // m_indices is GLuint now so keep this consistent.
        GLuint centerIdx = static_cast<GLuint>(m_vertices.size() / 6);
        m_vertices.push_back(0.0f); m_vertices.push_back(y); m_vertices.push_back(0.0f);
        m_vertices.push_back(0.0f); m_vertices.push_back(ny); m_vertices.push_back(0.0f);

        GLuint ringStart = centerIdx + 1;
        for (int j = 0; j <= m_segments; ++j) {
            float angle = j * angleStep;
            float x = cos(angle), z = sin(angle);
            m_vertices.push_back(x * m_radius);
            m_vertices.push_back(y);
            m_vertices.push_back(z * m_radius);
            m_vertices.push_back(0.0f);
            m_vertices.push_back(ny);
            m_vertices.push_back(0.0f);
        }

        for (int j = 0; j < m_segments; ++j) {
            if (ny > 0) {
                m_indices.push_back(centerIdx);
                m_indices.push_back(ringStart + j);
                m_indices.push_back(ringStart + j + 1);
            }
            else {
                m_indices.push_back(centerIdx);
                m_indices.push_back(ringStart + j + 1);
                m_indices.push_back(ringStart + j);
            }
        }
        };

    addCap(halfH, 1.0f);
    addCap(-halfH, -1.0f);

    m_meshDirty = false;
}

void Cylinder::draw() {
    float halfH = m_height / 2.0f;
    float angleStep = 2.0f * M_PI / m_segments;

    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= m_segments; ++i) {
        float angle = i * angleStep;
        float x = cos(angle), z = sin(angle);
        glNormal3f(x, 0.0f, z);
        glVertex3f(x * m_radius, -halfH, z * m_radius);
        glVertex3f(x * m_radius, halfH, z * m_radius);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, halfH, 0.0f);
    for (int i = 0; i <= m_segments; ++i) {
        float angle = i * angleStep;
        glVertex3f(cos(angle) * m_radius, halfH, sin(angle) * m_radius);
    }
    glEnd();

    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(0.0f, -halfH, 0.0f);
    for (int i = m_segments; i >= 0; --i) {
        float angle = i * angleStep;
        glVertex3f(cos(angle) * m_radius, -halfH, sin(angle) * m_radius);
    }
    glEnd();
}

Cylinder::~Cylinder() = default;