#include "sphere.h"
#include <QOpenGLFunctions>
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Sphere::setParameters(const QMap<QString, double>& params) {
    float newRadius = params.value("r", 1.0f);
    // FIX: guard against zero/negative radius producing degenerate/inverted
    // geometry with no warning.
    if (newRadius <= 0.0f) newRadius = 1.0f;

    if (newRadius != m_radius) {
        m_radius = newRadius;
        m_meshDirty = true;
    }
}

void Sphere::buildMesh() {
    m_vertices.clear();
    m_indices.clear();

    for (int i = 0; i <= m_stacks; ++i) {
        float phi = M_PI * (-0.5f + (float)i / m_stacks);
        float y = sin(phi);
        float r = cos(phi);

        for (int j = 0; j <= m_slices; ++j) {
            float theta = 2.0f * M_PI * (float)j / m_slices;
            float x = cos(theta);
            float z = sin(theta);

            // position
            m_vertices.push_back(x * r * m_radius);
            m_vertices.push_back(y * m_radius);
            m_vertices.push_back(z * r * m_radius);
            // normal (unit sphere -- same as position / radius)
            m_vertices.push_back(x * r);
            m_vertices.push_back(y);
            m_vertices.push_back(z * r);
        }
    }

    for (int i = 0; i < m_stacks; ++i) {
        for (int j = 0; j < m_slices; ++j) {
            int row0 = i * (m_slices + 1);
            int row1 = (i + 1) * (m_slices + 1);
            m_indices.push_back(row0 + j);
            m_indices.push_back(row1 + j);
            m_indices.push_back(row0 + j + 1);
            m_indices.push_back(row0 + j + 1);
            m_indices.push_back(row1 + j);
            m_indices.push_back(row1 + j + 1);
        }
    }

    m_meshDirty = false;
}

// Legacy immediate-mode fallback.
void Sphere::draw() {
    for (int i = 0; i < m_stacks; ++i) {
        // FIX: was `(i - 1) / m_stacks` for lat0, which at i==0 produces a
        // latitude below the south pole (< -pi/2), corrupting the first
        // strip. lat0/lat1 must bracket the same [i, i+1] band that
        // buildMesh() uses.
        float lat0 = M_PI * (-0.5f + (float)i / m_stacks);
        float lat1 = M_PI * (-0.5f + (float)(i + 1) / m_stacks);
        float z0 = sin(lat0), z1 = sin(lat1);
        float r0 = cos(lat0), r1 = cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= m_slices; ++j) {
            float lng = 2 * M_PI * (float)j / m_slices;
            float x = cos(lng), y = sin(lng);
            glNormal3f(x * r0, y * r0, z0);
            glVertex3f(x * r0 * m_radius, y * r0 * m_radius, z0 * m_radius);
            glNormal3f(x * r1, y * r1, z1);
            glVertex3f(x * r1 * m_radius, y * r1 * m_radius, z1 * m_radius);
        }
        glEnd();
    }
}

Sphere::~Sphere() = default;