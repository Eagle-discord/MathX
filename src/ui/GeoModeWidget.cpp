#include "GeoModeWidget.h"
#include "../render/RenderWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QFormLayout>
#include "../constants/Theme.h"
#include <QGroupBox>
#include <QComboBox>
#include <shapes/ShapeInfo.h>
#include <QCheckBox>

static void getParamRange(const QString& param, double& minVal, double& maxVal, double& defaultVal) {
    if (param == "r") { minVal = 0.1; maxVal = 10.0; defaultVal = 1.0; }
    else if (param == "s") { minVal = 0.1; maxVal = 5.0; defaultVal = 1.0; }
    else if (param == "w") { minVal = 0.1; maxVal = 10.0; defaultVal = 2.0; }
    else if (param == "h") { minVal = 0.1; maxVal = 10.0; defaultVal = 2.0; }
    else { minVal = 0.1; maxVal = 10.0; defaultVal = 1.0; }
}
GeoModeWidget::GeoModeWidget(QWidget* parent) : QWidget(parent) {
    setStyleSheet("background: #000000; border: none;");
    setContentsMargins(0, 0, 0, 0);

    // Render widget fills the entire widget
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    m_renderWidget = new RenderWidget(this);
    m_renderWidget->setStyleSheet("border: none; background: black;");
    mainLayout->addWidget(m_renderWidget, 1); // takes all space

    // Top bar overlay (shape selector) – child of render widget, floats on top
    m_topBar = new QWidget(m_renderWidget);
    m_topBar->setStyleSheet("background: rgba(0,0,0,150); border-radius: 8px;");
    QHBoxLayout* topLayout = new QHBoxLayout(m_topBar);
    topLayout->setContentsMargins(12, 6, 12, 6);
    QLabel* shapeLabel = new QLabel("Shape:");
    shapeLabel->setStyleSheet("color:#00e87a;");
    m_shapeSelector = new QComboBox;
    for (const auto& info : ALL_SHAPES_INFO) {
        m_shapeSelector->addItem(info.displayName);
    }
    connect(m_shapeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &GeoModeWidget::onShapeChanged);
    topLayout->addWidget(shapeLabel);
    topLayout->addWidget(m_shapeSelector);
    m_topBar->adjustSize();

    // Control panel – now a child of the render widget (overlay)
    m_controlPanel = new QWidget(m_renderWidget);
    m_controlPanel->setFixedWidth(280);
    m_controlPanel->setStyleSheet("background: transparent;"); // semi‑transparent
    QVBoxLayout* ctrlLayout = new QVBoxLayout(m_controlPanel);
    ctrlLayout->setContentsMargins(16, 20, 16, 20);
    ctrlLayout->setSpacing(16);

    // Title 
    // QLabel* title = new QLabel("Geometry Viewer");
    // title->setFont(Theme::monoFont(10, QFont::Bold));
    // title->setStyleSheet("color:#00e87a;");
    // ctrlLayout->addWidget(title, 0, Qt::AlignTop);

    m_slidersContainer = new QWidget(m_controlPanel);
    m_slidersLayout = new QFormLayout(m_slidersContainer);
    m_slidersLayout->setSpacing(12);
    ctrlLayout->addWidget(m_slidersContainer, 1);
    // Color controls
    QGroupBox* colorGroup = new QGroupBox("Shape Color");
    QFormLayout* colorLayout = new QFormLayout(colorGroup);

    auto addColorSlider = [&](const QString& name, QSlider*& slider, QLabel*& label) {
        slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue(100);
        label = new QLabel("1.00");
        label->setFont(Theme::monoFont(9, QFont::Bold));
        label->setStyleSheet("color:#00e87a;");
        label->setFixedWidth(40);
        QHBoxLayout* row = new QHBoxLayout;
        row->addWidget(new QLabel(name));
        row->addWidget(slider);
        row->addWidget(label);
        colorLayout->addRow(row);
        };
    addColorSlider("Red:", m_rSlider, m_rLabel);
    addColorSlider("Green:", m_gSlider, m_gLabel);
    addColorSlider("Blue:", m_bSlider, m_bLabel);

    connect(m_rSlider, &QSlider::valueChanged, this, [this](int val) {
        float f = val / 100.0f;
        m_rLabel->setText(QString::number(f, 'g', 2));
        m_renderWidget->setShapeColor(f, m_gSlider->value() / 100.0f, m_bSlider->value() / 100.0f);
        });
    connect(m_gSlider, &QSlider::valueChanged, this, [this](int val) {
        float f = val / 100.0f;
        m_gLabel->setText(QString::number(f, 'g', 2));
        m_renderWidget->setShapeColor(m_rSlider->value() / 100.0f, f, m_bSlider->value() / 100.0f);
        });
    connect(m_bSlider, &QSlider::valueChanged, this, [this](int val) {
        float f = val / 100.0f;
        m_bLabel->setText(QString::number(f, 'g', 2));
        m_renderWidget->setShapeColor(m_rSlider->value() / 100.0f, m_gSlider->value() / 100.0f, f);
        });

    // Background color (grayscale)
    QSlider* bgSlider = new QSlider(Qt::Horizontal);
    bgSlider->setRange(0, 100);
    bgSlider->setValue(0);
    QLabel* bgLabel = new QLabel("0.00");
    connect(bgSlider, &QSlider::valueChanged, [this, bgLabel](int val) {
        float f = val / 100.0f;
        bgLabel->setText(QString::number(f, 'g', 2));
        m_renderWidget->setBackgroundColor(f, f, f);
        });
    QHBoxLayout* bgRow = new QHBoxLayout;
    bgRow->addWidget(new QLabel("Background:"));
    bgRow->addWidget(bgSlider);
    bgRow->addWidget(bgLabel);
    colorLayout->addRow(bgRow);

    ctrlLayout->addWidget(colorGroup);
    // Back button – also a child of render widget (overlay)
    m_backButton = new QPushButton("Back", m_renderWidget);
    m_backButton->setFixedSize(100, 32);
    m_backButton->setStyleSheet(
        "QPushButton{background:#00e87a; color:#000; border-radius:6px;}"
        "QPushButton:hover{background:#00b860;}"
    );
    connect(m_backButton, &QPushButton::clicked, this, &GeoModeWidget::onBack);

    QCheckBox* mouseCtrl = new QCheckBox("Camera Control");
    mouseCtrl->setChecked(false);
    connect(mouseCtrl, &QCheckBox::toggled, this, [this](bool checked) {
        enableMouseCtrl(checked);
        });
    ctrlLayout->addWidget(mouseCtrl);

    QCheckBox* rotCheck = new QCheckBox("Auto-Rotate Shape");
    rotCheck->setChecked(true); // temporary, will be set per shape
    connect(rotCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_renderWidget->setRotationEnabled(checked);
        });
    ctrlLayout->addWidget(rotCheck);
    // Store pointer for later updates
    m_rotateCheckbox = rotCheck;
    // Set the horizontal offset for the renderer (to center the shape)

  
    
    int cpWidth = m_controlPanel->width(); // 280
    m_renderWidget->setHorizontalOffset(cpWidth / 2);
    // Shape selector
  /*  m_shapeSelector = new QComboBox;
    for (const auto& info : ALL_SHAPES_INFO) {
        m_shapeSelector->addItem(info.displayName);
    }

    connect(m_shapeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &GeoModeWidget::onShapeChanged);*/
   // ctrlLayout->addWidget(m_shapeSelector);
    // Initial positioning (will be updated in resizeEvent)
    resizeEvent(nullptr);
}
void GeoModeWidget::enableMouseCtrl(bool enabled) {
    m_mouseCtrl = enabled;
    m_renderWidget->setMouseCtrl(enabled);
}

void GeoModeWidget::rebuildSliders(const QStringList& paramNames, const QMap<QString, double>& params) {
    // Delete old container and create a new one
    if (m_slidersContainer) {
        delete m_slidersContainer;
        m_slidersContainer = nullptr;
    }
    m_slidersContainer = new QWidget(m_controlPanel);
    m_slidersLayout = new QFormLayout(m_slidersContainer);
    m_slidersLayout->setSpacing(12);

    // Insert the new container back into the control panel layout (after the title, before the color group)
    QVBoxLayout* ctrlLayout = qobject_cast<QVBoxLayout*>(m_controlPanel->layout());
    if (ctrlLayout) {
        // Assuming the title is at index 0, we insert at index 1 (or wherever appropriate)
        // In your code, the color group is added after the sliders container.
        // We'll remove the old container's reference and insert the new one.
        int oldIndex = ctrlLayout->indexOf(m_slidersContainer); // will be -1 after deletion
        // Insert at position 1 if the title is at index 0, and the color group is later.
        ctrlLayout->insertWidget(1, m_slidersContainer);
    }

    m_sliders.clear();
    m_labels.clear();

    for (const QString& name : paramNames) {
        double value = params.value(name, 1.0);
        QLabel* label = new QLabel(name + " =");
        label->setFont(Theme::monoFont(9));
        label->setStyleSheet("color:#5a6b5f;");

        QSlider* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        // Map parameter range (e.g., 0.1 to 10.0) to 0..100
        double minVal = 0.1, maxVal = 10.0;
        slider->setValue((value - minVal) / (maxVal - minVal) * 100);
        slider->setProperty("paramName", name);
        connect(slider, &QSlider::valueChanged, this, &GeoModeWidget::onSliderChanged);

        QLabel* valLabel = new QLabel(QString::number(value, 'g', 3));
        valLabel->setFont(Theme::monoFont(9, QFont::Bold));
        valLabel->setStyleSheet("color:#00e87a;");
        valLabel->setFixedWidth(50);
        valLabel->setAlignment(Qt::AlignRight);

        QHBoxLayout* row = new QHBoxLayout;
        row->addWidget(label);
        row->addWidget(slider);
        row->addWidget(valLabel);
        m_slidersLayout->addRow(row);

        m_sliders[name] = slider;
        m_labels[name] = valLabel;
    }
}

void GeoModeWidget::onShapeChanged(int index) {


    clearSliders();
    if (index < 0 || index >= ALL_SHAPES_INFO.size()) return;
    const ShapeInfo& info = ALL_SHAPES_INFO[index];
    m_shapeType = info.internalName;
    m_params = info.defaultParams;
 
    // Clear existing sliders and create new ones
    rebuildSliders(info.paramNames, m_params);
    if (m_rotateCheckbox) {
        m_rotateCheckbox->blockSignals(true);
        m_rotateCheckbox->setChecked(info.defaultRotate);
        m_rotateCheckbox->blockSignals(false);
        m_renderWidget->setRotationEnabled(info.defaultRotate);
    }
    updateShape();
}
void GeoModeWidget::resizeEvent(QResizeEvent*) {
    if (!m_controlPanel || !m_backButton) return;
    // Control panel on right
    int cpWidth = m_controlPanel->width();
    m_controlPanel->setGeometry(width() - cpWidth, 0, cpWidth, height());
    // Back button at bottom center
    m_backButton->move((width() - m_backButton->width()) / 2, height() - 50);
    // Top bar at top center
    if (m_topBar) {
        int tbWidth = m_topBar->width();
        m_topBar->move((width() - tbWidth) / 2, 10);
        m_topBar->raise(); // ensure it's on top of the OpenGL view
    }
}
void GeoModeWidget::setShape(const QString& type, const QMap<QString, double>& params) {
    m_shapeType = type;
    m_params = params;

    // Update combo box selection to match the shape type
    for (int i = 0; i < ALL_SHAPES_INFO.size(); ++i) {
        if (ALL_SHAPES_INFO[i].internalName == type) {
            // Block signals to avoid triggering onShapeChanged recursively
            m_shapeSelector->blockSignals(true);
            m_shapeSelector->setCurrentIndex(i);
            m_shapeSelector->blockSignals(false);
            break;
        }
    }

    rebuildSliders(params.keys(), m_params);
    updateShape();
}
void GeoModeWidget::clearSliders() {
    // Remove all rows from the form layout
    delete m_slidersLayout;
    m_slidersLayout = nullptr;
    m_sliders.clear();
    m_labels.clear();

    // Create a new layout
    m_slidersLayout = new QFormLayout(slidersContainer);
    m_slidersLayout->setSpacing(12);

}


void GeoModeWidget::createSliders(const QStringList& paramNames) {
    // Remove existing slider widgets from layout and delete them
    clearSliders();
    for (const QString& name : paramNames) {
        double minVal, maxVal, defaultVal;
        getParamRange(name, minVal, maxVal, defaultVal);
        double val = m_params.value(name, defaultVal);

        QLabel* label = new QLabel(name + " =");
        label->setFont(Theme::monoFont(9));
        label->setStyleSheet("color:#5a6b5f;bcakground:#000000");

        QSlider* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue((val - minVal) / (maxVal - minVal) * 100);
        slider->setProperty("paramName", name);
           connect(slider, &QSlider::valueChanged, this, &GeoModeWidget::onSliderChanged);
           slider->setStyleSheet(QString("background:#000000"));
        QLabel* valLabel = new QLabel(QString::number(val, 'g', 3));
        valLabel->setFont(Theme::monoFont(9, QFont::Bold));
        valLabel->setStyleSheet("color:#00e87a;background:#000000");
        valLabel->setFixedWidth(50);
        valLabel->setAlignment(Qt::AlignRight);
        QHBoxLayout* row = new QHBoxLayout;
        row->addWidget(label);
        row->addWidget(slider);
        row->addWidget(valLabel);
        m_slidersLayout->addRow(row);
       
        m_sliders[name] = slider;
        m_labels[name] = valLabel;
    }
}

void GeoModeWidget::onSliderChanged() {
    QSlider* s = qobject_cast<QSlider*>(sender());
    if (!s) return;
    QString name = s->property("paramName").toString();
    double minVal, maxVal, dummy;
    getParamRange(name, minVal, maxVal, dummy);
    double val = minVal + (s->value() / 100.0) * (maxVal - minVal);
    m_params[name] = val;
    if (m_labels.contains(name))
        m_labels[name]->setText(QString::number(val, 'g', 3));
    updateShape();
}

void GeoModeWidget::updateShape() {
    m_renderWidget->setShape(m_shapeType, m_params);
}

void GeoModeWidget::onBack() {
    
    emit backClicked();
}
