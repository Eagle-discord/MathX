#include "RenderWidget.h"
#include "OpenGLRenderer.h"
#include "VulkanRenderer.h"
#include <QVBoxLayout>

RenderWidget::RenderWidget(QWidget* parent) : QWidget(parent) {
    m_horizontalOffset = 0;
    // Try Vulkan first (stub always returns false)
    m_renderer = std::make_unique<VulkanRenderer>();
    if (!m_renderer->initialize(this)) {
        // Fall back to OpenGL
        m_renderer = std::make_unique<OpenGLRenderer>();
        m_renderer->initialize(this);
    }
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_renderer->widget());
    if (dynamic_cast<OpenGLRenderer*>(m_renderer.get()))
        qDebug() << "RenderWidget: Using OpenGL backend";
    else if (dynamic_cast<VulkanRenderer*>(m_renderer.get()))
        qDebug() << "RenderWidget: Using Vulkan backend (stub)";
}

RenderWidget::~RenderWidget() {

    qDebug() << "~RenderWidget";
};

void RenderWidget::setShape(const QString& type, const QMap<QString, double>& params) {
    m_renderer->setShape(type, params);
}

void RenderWidget::setDarkTheme(bool dark) {
    m_renderer->setDarkTheme(dark);
}

void RenderWidget::setMouseCtrl(bool enabled) {
    m_renderer->setMouseCtrl(enabled);
}

void RenderWidget::clearMeshes()
{
    m_renderer->clearMesh();
    
}

void RenderWidget::setRotationEnabled(bool enabled) {
    m_renderer->setRotationEnabled(enabled);
}

void RenderWidget::setHorizontalOffset(int offsetPixels) {
    m_horizontalOffset = offsetPixels;
    if (auto* gl = dynamic_cast<OpenGLRenderer*>(m_renderer.get())) {
        gl->setHorizontalOffset(offsetPixels);
    }
}
void RenderWidget::setShapeColor(float r, float g, float b) {
    if (auto* gl = dynamic_cast<OpenGLRenderer*>(m_renderer.get()))
        gl->setShapeColor(r, g, b);
}
void RenderWidget::setPropertyLabelsVisible(bool on) {
    if (auto* gl = dynamic_cast<OpenGLRenderer*>(m_renderer.get()))
        gl->setPropertyLabelsVisible(on);
}

void RenderWidget::startFormulaAnimation(const QString& shapeType, const QString& formulaKey) {
    if (auto* gl = dynamic_cast<OpenGLRenderer*>(m_renderer.get()))
        gl->startFormulaAnimation(shapeType, formulaKey);
}
void RenderWidget::stopFormulaAnimation() {
    if (auto* gl = dynamic_cast<OpenGLRenderer*>(m_renderer.get()))
        gl->stopFormulaAnimation();
}
void RenderWidget::setBackgroundColor(float r, float g, float b) {
    if (auto* gl = dynamic_cast<OpenGLRenderer*>(m_renderer.get()))
        gl->setBackgroundColor(r, g, b);
}