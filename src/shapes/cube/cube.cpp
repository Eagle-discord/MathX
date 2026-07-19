#include "cube.h"
#include <QOpenGLFunctions>  // for glVertex3f, etc.
// Note: OpenGL includes are already handled by the renderer's context.


void Cube::setParameters(const QMap<QString, double>& params) {
    float newSize = params.value("s", 1.0f);
    if (newSize != m_size) {
        m_size = newSize;
        m_meshDirty = true;
    }
}

void Cube::buildMesh() {
    float h = m_size * 0.5f;

    m_vertices = {
        -h,-h,-h,  h,-h,-h,  h, h,-h, -h, h,-h,  // back
        -h,-h, h,  h,-h, h,  h, h, h, -h, h, h,  // front
    };

    m_indices = {
        0,1, 1,2, 2,3, 3,0,  // back face
        4,5, 5,6, 6,7, 7,4,  // front face
        0,4, 1,5, 2,6, 3,7   // connecting edges
    };

    m_indexCount = (GLsizei)m_indices.size();
    m_meshDirty = false;
}

void Cube::uploadMesh() {
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
void Cube::draw() {
    qDebug("Cube draw");
    float h = m_size / 2.0f;

    glBegin(GL_QUADS);
    // Front
    glNormal3f(0, 0, 1);
    glVertex3f(-h, -h, h); glVertex3f(h, -h, h);
    glVertex3f(h, h, h); glVertex3f(-h, h, h);
    // Back
    glNormal3f(0, 0, -1);
    glVertex3f(h, -h, -h); glVertex3f(-h, -h, -h);
    glVertex3f(-h, h, -h); glVertex3f(h, h, -h);
    // Left
    glNormal3f(-1, 0, 0);
    glVertex3f(-h, -h, -h); glVertex3f(-h, -h, h);
    glVertex3f(-h, h, h); glVertex3f(-h, h, -h);
    // Right
    glNormal3f(1, 0, 0);
    glVertex3f(h, -h, h); glVertex3f(h, -h, -h);
    glVertex3f(h, h, -h); glVertex3f(h, h, h);
    // Top
    glNormal3f(0, 1, 0);
    glVertex3f(-h, h, h); glVertex3f(h, h, h);
    glVertex3f(h, h, -h); glVertex3f(-h, h, -h);
    // Bottom
    glNormal3f(0, -1, 0);
    glVertex3f(-h, -h, -h); glVertex3f(h, -h, -h);
    glVertex3f(h, -h, h); glVertex3f(-h, -h, h);
    glEnd();
}
void Cube::drawShader(QOpenGLShaderProgram& shader) {
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
Cube::~Cube() {
    qDebug("~Cube");
}