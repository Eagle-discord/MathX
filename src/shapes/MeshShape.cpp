#include "MeshShape.h"
#include <QOpenGLFunctions>
#include <QDebug>

MeshShape::~MeshShape() {
    // FIX: previously each subclass destructor just did qDebug() and
    // relied on Qt to "automatically" clean up the VAO/VBO/EBO. That's
    // fragile — it only works if a valid, current GL context still
    // exists at destruction time. Explicitly destroy() while a context
    // is current; skip it (rather than crash) if one isn't, since that
    // means the GPU objects are already gone with their context anyway.
    if (QOpenGLContext::currentContext()) {
        if (m_vbo.isCreated()) m_vbo.destroy();
        if (m_ebo.isCreated()) m_ebo.destroy();
        if (m_vao.isCreated()) m_vao.destroy();
    }
}

void MeshShape::uploadMesh() {
    if (!m_vao.isCreated()) m_vao.create();
    m_vao.bind();

    if (!m_vbo.isCreated()) m_vbo.create();
    m_vbo.bind();
    m_vbo.allocate(m_vertices.data(),
        static_cast<int>(m_vertices.size() * sizeof(float)));

    if (!m_ebo.isCreated()) m_ebo.create();
    m_ebo.bind();
    m_ebo.allocate(m_indices.data(),
        static_cast<int>(m_indices.size() * sizeof(GLuint)));

    m_vao.release();

    // Free CPU-side data now that it's on the GPU.
    m_vertices.clear();
    m_vertices.shrink_to_fit();
    m_indices.clear();
    m_indices.shrink_to_fit();
}

void MeshShape::ensureUploaded(QOpenGLShaderProgram& shader) {
    if (!m_meshDirty) return;

    buildMesh();

    m_posLoc = shader.attributeLocation("position");
    if (hasNormals())
        m_normalLoc = shader.attributeLocation("normal");

    // FIX: warn (once, not every frame) if a shape's shader doesn't
    // expose the attributes it needs — this used to fail silently,
    // leaving you to guess why nothing rendered.
    if (m_posLoc < 0)
        qWarning("MeshShape: shader has no 'position' attribute");
    if (hasNormals() && m_normalLoc < 0)
        qWarning("MeshShape: shader has no 'normal' attribute");

    // Cache index count BEFORE uploadMesh() clears m_indices.
    m_indexCount = static_cast<GLsizei>(m_indices.size());

    uploadMesh();
}

void MeshShape::bindForDraw(QOpenGLShaderProgram& shader) {
    m_vao.bind();

    const int stride = hasNormals() ? 6 * sizeof(float) : 3 * sizeof(float);

    if (m_posLoc >= 0) {
        shader.enableAttributeArray(m_posLoc);
        shader.setAttributeBuffer(m_posLoc, GL_FLOAT, 0, 3, stride);
    }
    if (hasNormals() && m_normalLoc >= 0) {
        shader.enableAttributeArray(m_normalLoc);
        shader.setAttributeBuffer(m_normalLoc, GL_FLOAT, 3 * sizeof(float), 3, stride);
    }
}

void MeshShape::unbindAfterDraw(QOpenGLShaderProgram& shader) {
    if (m_posLoc >= 0)
        shader.disableAttributeArray(m_posLoc);
    if (hasNormals() && m_normalLoc >= 0)
        shader.disableAttributeArray(m_normalLoc);
    m_vao.release();
}

void MeshShape::drawShader(QOpenGLShaderProgram& shader) {
    ensureUploaded(shader);
    bindForDraw(shader);
    glDrawElements(drawMode(), m_indexCount, GL_UNSIGNED_INT, nullptr);
    unbindAfterDraw(shader);
}