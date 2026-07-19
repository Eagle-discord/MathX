#include "cuboid.h"

void Cuboid::setParameters(const QMap<QString, double>& params) {
    float newW = params.value("w", 1.0f);
    float newH = params.value("h", 1.0f);
    float newD = params.value("l", 1.0f);
    if (newW != m_width || newH != m_height || newD != m_depth) {
        m_width = newW;
        m_height = newH;
        m_depth = newD;
        m_meshDirty = true;
    }
}

void Cuboid::buildMesh() {
    float hw = m_width * 0.5f;
    float hh = m_height * 0.5f;
    float hd = m_depth * 0.5f;

    // 8 unique vertices
    m_vertices = {
        -hw, -hh, -hd,   // 0 back-bottom-left
         hw, -hh, -hd,   // 1 back-bottom-right
         hw,  hh, -hd,   // 2 back-top-right
        -hw,  hh, -hd,   // 3 back-top-left
        -hw, -hh,  hd,   // 4 front-bottom-left
         hw, -hh,  hd,   // 5 front-bottom-right
         hw,  hh,  hd,   // 6 front-top-right
        -hw,  hh,  hd,   // 7 front-top-left
    };

    // 12 edges = 24 indices
    m_indices = {
        0,1, 1,2, 2,3, 3,0,  // back face
        4,5, 5,6, 6,7, 7,4,  // front face
        0,4, 1,5, 2,6, 3,7   // connecting edges
    };

    m_indexCount = (GLsizei)m_indices.size();
    m_meshDirty = false;
}

void Cuboid::uploadMesh() {
    if (!m_vao.isCreated()) m_vao.create();
    m_vao.bind();

    m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(m_vertices.data(), m_vertices.size() * sizeof(float));

    m_ebo.create();
    m_ebo.bind();
    m_ebo.allocate(m_indices.data(), m_indices.size() * sizeof(int));

    m_vao.release();

    m_vertices.clear(); m_vertices.shrink_to_fit();
    m_indices.clear();  m_indices.shrink_to_fit();
}

void Cuboid::drawShader(QOpenGLShaderProgram& shader) {
    if (m_meshDirty) {
        buildMesh();
        m_posLoc = shader.attributeLocation("position");
        uploadMesh();
    }

    m_vao.bind();
    shader.enableAttributeArray(m_posLoc);
    shader.setAttributeBuffer(m_posLoc, GL_FLOAT, 0, 3, 3 * sizeof(float));

    glDrawElements(GL_LINES, m_indexCount, GL_UNSIGNED_INT, nullptr);

    shader.disableAttributeArray(m_posLoc);
    m_vao.release();
}

void Cuboid::draw() {
    float hw = m_width * 0.5f;
    float hh = m_height * 0.5f;
    float hd = m_depth * 0.5f;

    glBegin(GL_QUADS);
    // Front
    glNormal3f(0, 0, 1);
    glVertex3f(-hw, -hh, hd); glVertex3f(hw, -hh, hd);
    glVertex3f(hw, hh, hd);   glVertex3f(-hw, hh, hd);
    // Back
    glNormal3f(0, 0, -1);
    glVertex3f(hw, -hh, -hd); glVertex3f(-hw, -hh, -hd);
    glVertex3f(-hw, hh, -hd); glVertex3f(hw, hh, -hd);
    // Left
    glNormal3f(-1, 0, 0);
    glVertex3f(-hw, -hh, -hd); glVertex3f(-hw, -hh, hd);
    glVertex3f(-hw, hh, hd);   glVertex3f(-hw, hh, -hd);
    // Right
    glNormal3f(1, 0, 0);
    glVertex3f(hw, -hh, hd);  glVertex3f(hw, -hh, -hd);
    glVertex3f(hw, hh, -hd);  glVertex3f(hw, hh, hd);
    // Top
    glNormal3f(0, 1, 0);
    glVertex3f(-hw, hh, hd);  glVertex3f(hw, hh, hd);
    glVertex3f(hw, hh, -hd);  glVertex3f(-hw, hh, -hd);
    // Bottom
    glNormal3f(0, -1, 0);
    glVertex3f(-hw, -hh, -hd); glVertex3f(hw, -hh, -hd);
    glVertex3f(hw, -hh, hd);   glVertex3f(-hw, -hh, hd);
    glEnd();
}

Cuboid::~Cuboid() {}