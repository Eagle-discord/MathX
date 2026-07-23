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
#include <QVBoxLayout>

class RenderWidget;
class GeoCard;
class PropertiesPanel;

class GeoModeWidget : public QWidget {
    Q_OBJECT
public:
    explicit GeoModeWidget(QWidget* parent = nullptr);
    ~GeoModeWidget();
    void enableMouseCtrl(bool enabled);
    void setShape(const QString& type, const QMap<QString, double>& params);

signals:
    void backClicked();
private slots:
    void onSliderChanged();
    void onBack();
private:
    void clearSliders();
    void onShapeChanged(int index);
    void createSliders(const QStringList& paramNames);
    void resizeEvent(QResizeEvent* event) override;
    void updateShape();

    bool m_mouseCtrl = false;
    int  m_propsPanelWidth = 260;
    void updatePropsPanelWidth();
    // Single builder for the properties card. BOTH card-rebuild paths
    // (onShapeChanged and setShape) must use this - a card built without the
    // formula connections silently breaks the walkthrough trigger.
    void rebuildPropsCard();
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
    RenderWidget* m_renderWidget = nullptr;
    // Geometry settings that had no consumer until now: applied on init and
    // kept in step with the settings page afterwards.
    void applyGeometrySettings();
    void wireGeometrySettings();
    QMap<QString, QSlider*> m_sliders;
    QMap<QString, QLabel*>  m_labels;
    QMap<QString, double>   m_params;
    QFormLayout* m_slidersLayout = nullptr;
    QVBoxLayout* m_ctrlLayout = nullptr;
    QString      m_shapeType;
    QComboBox* m_shapeSelector = nullptr;
    QWidget* m_topBar = nullptr;

    // Properties panel
    PropertiesPanel* m_propertiesPanel = nullptr;
    QVBoxLayout* m_propsPanelLayout = nullptr;
    GeoCard* m_propsCard = nullptr;
};