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

    
private:

    std::unique_ptr<Renderer> m_renderer;
    int m_horizontalOffset;
};