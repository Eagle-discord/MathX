#include "GeoModeWidget.h"
#include "../render/RenderWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSlider>
#include <QFormLayout>
#include "../constants/Theme.h"

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
    
    // Main vertical layout (render + controls on top, back button at bottom)
    QVBoxLayout* verticalLayout = new QVBoxLayout(this);
    verticalLayout->setContentsMargins(0, 0, 0, 0);
    verticalLayout->setSpacing(0);

    // Content row: render widget (left) + control panel (right)
    QHBoxLayout* contentLayout = new QHBoxLayout;
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // Render area (takes all available space)
    m_renderWidget = new RenderWidget(this);
    m_renderWidget->setStyleSheet("border: none; background: black;");
   
    contentLayout->addWidget(m_renderWidget, 1);

    // Control panel (sliders + title)
    controlPanel = new QWidget(this);
    controlPanel->setFixedWidth(280);
    controlPanel->setStyleSheet("background:#000000;");  // black, no border
    QVBoxLayout* ctrlLayout = new QVBoxLayout(controlPanel);
    ctrlLayout->setContentsMargins(16, 20, 16, 20);
    ctrlLayout->setSpacing(16);
    int cpWidth = controlPanel->width();
    m_renderWidget->setHorizontalOffset(cpWidth);
    // Optional: Title (commented out as you removed it)
    // QLabel* title = new QLabel("Geometry Viewer");
    // title->setFont(Theme::monoFont(10, QFont::Bold));
    // title->setStyleSheet("color:#00e87a;");
    // ctrlLayout->addWidget(title, 0, Qt::AlignTop);

    QWidget* slidersContainer = new QWidget;
    m_slidersLayout = new QFormLayout(slidersContainer);
    m_slidersLayout->setSpacing(12);
    ctrlLayout->addWidget(slidersContainer, 1);

    contentLayout->addWidget(controlPanel);
    verticalLayout->addLayout(contentLayout, 1); // give all extra space to content

    // Bottom bar with centered Back button
    QWidget* bottomBar = new QWidget(this);
    bottomBar->setFixedHeight(60);
    bottomBar->setStyleSheet("background: #000000; border: none;");
    QHBoxLayout* bottomLayout = new QHBoxLayout(bottomBar);
    bottomLayout->setAlignment(Qt::AlignCenter);
    QPushButton* backBtn = new QPushButton("Back");
    backBtn->setFixedSize(100, 32);
    backBtn->setStyleSheet(
        "QPushButton{background:#00e87a; color:#000; border-radius:6px;}"
        "QPushButton:hover{background:#00b860;}"
    );
    connect(backBtn, &QPushButton::clicked, this, &GeoModeWidget::onBack);
    bottomLayout->addWidget(backBtn);
    verticalLayout->addWidget(bottomBar);
}
void GeoModeWidget::setShape(const QString& type, const QMap<QString, double>& params) {
    m_shapeType = type;
    m_params = params;
    createSliders(params.keys());
    updateShape();
}

void GeoModeWidget::createSliders(const QStringList& paramNames) {
    // Clean up old widgets
    for (QSlider* s : m_sliders) delete s;
    for (QLabel* l : m_labels) delete l;
    m_sliders.clear();
    m_labels.clear();

    QLayoutItem* item;
    while ((item = m_slidersLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }

    for (const QString& name : paramNames) {
        double minVal, maxVal, defaultVal;
        getParamRange(name, minVal, maxVal, defaultVal);
        double val = m_params.value(name, defaultVal);

        QLabel* label = new QLabel(name + " =");
        label->setFont(Theme::monoFont(9));
        label->setStyleSheet("color:#5a6b5f;");

        QSlider* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue((val - minVal) / (maxVal - minVal) * 100);
        slider->setProperty("paramName", name);
           connect(slider, &QSlider::valueChanged, this, &GeoModeWidget::onSliderChanged);

        QLabel* valLabel = new QLabel(QString::number(val, 'g', 3));
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
