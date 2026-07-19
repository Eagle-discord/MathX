#pragma once
#include "RenderShape.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLContext>
#include <vector>

// Common plumbing shared by all indexed-mesh primitives (Sphere, Cube,
// Cylinder, Circle, ...). Subclasses only need to implement buildMesh(),
// draw() (immediate-mode fallback), setParameters(), and describe their
// vertex layout via hasNormals()/drawMode().
//
// This exists to fix a class of bugs that were previously duplicated
// four times over: GLuint/int index mismatch, missing copy protection
// on GL handle owners, and no attribute-location validation.
class MeshShape : public RenderShape {
public:
    void drawShader(QOpenGLShaderProgram& shader) override;

protected:
    MeshShape() = default;
    ~MeshShape() override;

    // Subclass fills m_vertices / m_indices and clears m_meshDirty.
    virtual void buildMesh() = 0;

    // Vertex layout description — override if a shape differs from the
    // default (position + normal, interleaved, drawn as triangles).
    virtual bool  hasNormals() const { return true; }
    virtual GLenum drawMode()  const { return GL_TRIANGLES; }

    // Upload CPU buffers to the GPU and free the CPU copies.
    void uploadMesh();

    // Bind VAO + enable/point vertex attributes. Call before issuing your
    // own glDrawElements() if you need custom multi-pass drawing (see
    // Circle's outline pass). Safe to call multiple times per frame.
    void bindForDraw(QOpenGLShaderProgram& shader);
    void unbindAfterDraw(QOpenGLShaderProgram& shader);

    // Rebuilds + re-uploads the mesh if m_meshDirty is set, caching
    // attribute locations. Call this at the top of any custom
    // drawShader() override before bindForDraw().
    void ensureUploaded(QOpenGLShaderProgram& shader);

    std::vector<float> m_vertices;
    std::vector<GLuint> m_indices;   // FIX: was std::vector<int> in every
    // subclass; GLuint is what
    // glDrawElements(..., GL_UNSIGNED_INT, ...)
    // actually expects.
    bool    m_meshDirty = true;
    GLsizei m_indexCount = 0;
    int     m_posLoc = -1;
    int     m_normalLoc = -1;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer m_ebo{ QOpenGLBuffer::IndexBuffer };

private:
    // FIX: none of the original classes disabled copying. QOpenGLBuffer /
    // QOpenGLVertexArrayObject wrap a raw GL handle with no refcounting;
    // copying a shape would let two objects think they own (and destroy)
    // the same GPU resource. Disallow it outright.
    MeshShape(const MeshShape&) = delete;
    MeshShape& operator=(const MeshShape&) = delete;
};