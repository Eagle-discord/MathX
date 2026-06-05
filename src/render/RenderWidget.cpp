#include "RenderWidget.h"
#include "OpenGLRenderer.h"
#include "VulkanRenderer.h"
#include <QVBoxLayout>

RenderWidget::RenderWidget(QWidget* parent) : QWidget(parent) {
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

RenderWidget::~RenderWidget() = default;

void RenderWidget::setShape(const QString& type, const QMap<QString, double>& params) {
    m_renderer->setShape(type, params);
}

void RenderWidget::setDarkTheme(bool dark) {
    m_renderer->setDarkTheme(dark);
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