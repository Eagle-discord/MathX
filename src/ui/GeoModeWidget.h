#pragma once
#include <QWidget>
#include <QMap>
#include <QString>
#include <QSlider>
#include <QLabel>
#include <QFormLayout>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>

class RenderWidget;

class GeoModeWidget : public QWidget {
    Q_OBJECT
public:
    explicit GeoModeWidget(QWidget* parent = nullptr);

    
    void enableMouseCtrl(bool enabled);
    void setShape(const QString& type, const QMap<QString, double>& params);

    

signals:
    void backClicked();

private slots:
    void onSliderChanged();
    void onBack();


private:
    void rebuildSliders(const QStringList& paramNames, const QMap<QString, double>& params);
    void clearSliders();
    void onShapeChanged(int index);
    void createSliders(const QStringList& paramNames);
    void resizeEvent(QResizeEvent* event) override;
    void updateShape();
    
    bool m_mouseCtrl = false;
    QCheckBox* m_rotateCheckbox = nullptr;
    QWidget* m_slidersContainer = nullptr;
    QSlider* m_rSlider = nullptr;
    QSlider* m_gSlider = nullptr;
    QSlider* m_bSlider = nullptr;
    QLabel* m_rLabel = nullptr;
    QLabel* m_gLabel = nullptr;
    QLabel* m_bLabel = nullptr;
    QWidget* m_controlPanel = nullptr;
    QCheckBox* m_mouseCtrlEnabled = nullptr;
    QPushButton* m_backButton = nullptr;
    QWidget* controlPanel = nullptr;
    RenderWidget* m_renderWidget = nullptr;
    QMap<QString, QSlider*> m_sliders;
    QMap<QString, QLabel*> m_labels;
    QMap<QString, double> m_params;
    QFormLayout* m_slidersLayout = nullptr;
    QString m_shapeType;
    QComboBox* m_shapeSelector = nullptr;
    QWidget* m_topBar = nullptr;
    QWidget* slidersContainer = nullptr;
};