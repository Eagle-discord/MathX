#pragma once
#include "../RenderShape.h"
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <vector>

class Cuboid : public RenderShape {
public:
    void setParameters(const QMap<QString, double>& params) override;
    void draw() override;
    void drawShader(QOpenGLShaderProgram& shader) override;
    ~Cuboid();

private:
    void buildMesh();
    void uploadMesh();

    float m_width = 1.0f;
    float m_height = 1.0f;
    float m_depth = 1.0f;
    bool  m_meshDirty = true;

    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer m_vbo{ QOpenGLBuffer::VertexBuffer };
    QOpenGLBuffer m_ebo{ QOpenGLBuffer::IndexBuffer };

    std::vector<float> m_vertices;
    std::vector<int>   m_indices;
    GLsizei m_indexCount = 0;
    int m_posLoc = -1;
};