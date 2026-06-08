#pragma once
#include <QWidget>
#include <memory>

class Renderer;

class RenderWidget : public QWidget {
    Q_OBJECT;
        
public:
    explicit RenderWidget(QWidget* parent = nullptr);
    ~RenderWidget();

    void setShape(const QString& type, const QMap<QString, double>& params);
    void setDarkTheme(bool dark);
    void setRotationEnabled(bool enabled);
    void setHorizontalOffset(int offsetPixels);
    void setShapeColor(float r, float g, float b);
    void setBackgroundColor(float r, float g, float b);
    void setMouseCtrl(bool enabled);
private:

    std::unique_ptr<Renderer> m_renderer;
    int m_horizontalOffset;
};