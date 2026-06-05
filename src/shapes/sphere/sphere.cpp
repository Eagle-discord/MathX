#include "Sphere.h"
#include <cmath>
#include <qopenglfunctions.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


void Sphere::setParameters(const QMap<QString, double>& params) {
    m_radius = params.value("r", 1.0f);

}

void Sphere::draw() {
    for (int i = 0; i <= m_stacks; ++i) {
        float lat0 = M_PI * (-0.5f + (float)(i - 1) / m_stacks);
        float lat1 = M_PI * (-0.5f + (float)i / m_stacks);
        float z0 = m_radius * sin(lat0);
        float z1 = m_radius * sin(lat1);
        float r0 = m_radius * cos(lat0);
        float r1 = m_radius * cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= m_slices; ++j) {
            float lng = 2 * M_PI * (float)j / m_slices;
            float x = cos(lng);
            float y = sin(lng);
            glVertex3f(x * r0, y * r0, z0);
            glVertex3f(x * r1, y * r1, z1);
        }
        glEnd();
    }
}