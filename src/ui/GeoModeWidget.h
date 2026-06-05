#pragma once
#include <QWidget>
#include <QMap>
#include <QString>
#include <QSlider>
#include <QLabel>
#include <QFormLayout>

class RenderWidget;

class GeoModeWidget : public QWidget {
    Q_OBJECT
public:
    explicit GeoModeWidget(QWidget* parent = nullptr);
    void setShape(const QString& type, const QMap<QString, double>& params);

signals:
    void backClicked();

private slots:
    void onSliderChanged();
    void onBack();


private:
    void createSliders(const QStringList& paramNames);

    void updateShape();
    QWidget* controlPanel = nullptr;
    RenderWidget* m_renderWidget = nullptr;
    QMap<QString, QSlider*> m_sliders;
    QMap<QString, QLabel*> m_labels;
    QMap<QString, double> m_params;
    QFormLayout* m_slidersLayout = nullptr; // added
    QString m_shapeType;
};