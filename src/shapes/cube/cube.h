#pragma once
#include "../RenderShape.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <vector>

class Cube : public RenderShape {
public:
    void setParameters(const QMap<QString, double>& params) override;
    void draw() override;
    void drawShader(QOpenGLShaderProgram& shader) override;
    ~Cube();

private:
    void buildMesh();
    void uploadMesh();

    float m_size = 1.0f;
    bool  m_meshDirty = true;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer m_ebo{ QOpenGLBuffer::IndexBuffer };

    std::vector<float> m_vertices;
    std::vector<int>   m_indices;
    GLsizei m_indexCount = 0;
    int m_posLoc = -1;
};