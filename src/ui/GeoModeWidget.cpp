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
#include "PropertiesPanel.h"
#include "../shapes/GeoCard.h"
#include "../shapes/ShapeDefs.h"
#include <algorithm>

// FIX: the properties panel width used to be the literal `260` hardcoded
// in TWO places (the constructor's setFixedWidth call, and resizeEvent's
// setGeometry call). If you ever changed one you'd silently desync the
// other. Now there's one source of truth: m_propsPanelWidth (declared in
// the header), recomputed by updatePropsPanelWidth() and read everywhere
// else. kPropsPanelMinWidth/kPropsPanelMaxWidth bound how far it can grow.
static constexpr int kPropsPanelMinWidth = 260;
static constexpr int kPropsPanelMaxWidth = 420;
static constexpr int kPropsPanelMargin = 24; // card's own inner padding

static void getParamRange(const QString& shapeName, const QString& param,
    double& minVal, double& maxVal, double& defaultVal) {
    const ShapeDef* def = findShapeDef(shapeName);
    if (def) {
        const ParamDef* p = def->findParam(param);
        if (p) {
            minVal = p->min;
            maxVal = p->max;
            defaultVal = p->defaultVal;
            return;
        }
    }
    // fallback
    minVal = 0.1; maxVal = 10.0; defaultVal = 1.0;
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
    for (const auto& info : ALL_SHAPES_INFO()) {
        m_shapeSelector->addItem(info.displayName);
    }
    connect(m_shapeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &GeoModeWidget::onShapeChanged);
    topLayout->addWidget(shapeLabel);
    topLayout->addWidget(m_shapeSelector);
    m_topBar->adjustSize();

    // Control panel – now a child of the render widget (overlay)
    m_controlPanel = new QWidget(m_renderWidget);
    m_controlPanel->setFixedWidth(280);
    m_controlPanel->setStyleSheet("background: transparent;"); // semi‑transparent
    m_ctrlLayout = new QVBoxLayout(m_controlPanel);
    m_ctrlLayout->setContentsMargins(16, 20, 16, 20);
    m_ctrlLayout->setSpacing(16);

    m_slidersContainer = new QWidget(m_controlPanel);
    m_slidersLayout = new QFormLayout(m_slidersContainer);
    m_slidersLayout->setSpacing(12);
    m_ctrlLayout->addWidget(m_slidersContainer, 1);
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

    // Default to pure white (the glow tints it); sync labels + renderer.
    m_rSlider->setValue(100);
    m_gSlider->setValue(100);
    m_bSlider->setValue(100);

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

    m_ctrlLayout->addWidget(colorGroup);
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
    m_ctrlLayout->addWidget(mouseCtrl);

    QCheckBox* rotCheck = new QCheckBox("Auto-Rotate Shape");
    rotCheck->setChecked(true); // temporary, will be set per shape
    connect(rotCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_renderWidget->setRotationEnabled(checked);

        });
    m_ctrlLayout->addWidget(rotCheck);
    // Store pointer for later updates
    m_rotateCheckbox = rotCheck;
    // Set the horizontal offset for the renderer (to center the shape)
    if (m_propsCard)m_propsCard->deleteLater();
    if(m_propertiesPanel)m_propertiesPanel->deleteLater();
    m_propertiesPanel = new PropertiesPanel(m_renderWidget);

    m_propsPanelWidth = kPropsPanelMinWidth;
    m_propertiesPanel->setFixedWidth(m_propsPanelWidth);
    m_propsPanelLayout = m_propertiesPanel->contentLayout();

    // Initial positioning (will be updated in resizeEvent)
    resizeEvent(nullptr);
    // Force initial shape load
    onShapeChanged(m_shapeSelector->currentIndex());

}
void GeoModeWidget::enableMouseCtrl(bool enabled) {
    m_mouseCtrl = enabled;
    m_renderWidget->setMouseCtrl(enabled);
}

// FIX (new): centralizes properties-panel sizing. Queries the current
// props card's sizeHint() — which now reflects the widest ResultRow's
// natural, unwrapped width — and grows the panel to fit, clamped to
// [kPropsPanelMinWidth, kPropsPanelMaxWidth]. This replaces the old
// approach of forcing a fixed 260px and letting long formula text clip.
void GeoModeWidget::updatePropsPanelWidth() {
    if (!m_propertiesPanel) return;

    int desired = kPropsPanelMinWidth;
    if (m_propsCard)
        desired = std::max(desired, m_propsCard->sizeHint().width() + kPropsPanelMargin);
    desired = std::min(desired, kPropsPanelMaxWidth);

    if (desired == m_propsPanelWidth) return;

    m_propsPanelWidth = desired;
    m_propertiesPanel->setFixedWidth(m_propsPanelWidth);
    resizeEvent(nullptr); // reposition immediately with the new width
}

void GeoModeWidget::onShapeChanged(int index) {
    clearSliders();
    const auto& shapes = ALL_SHAPES_INFO();
    if (index < 0 || index >= shapes.size()) return;
    const ShapeInfo& info = shapes[index];
    m_shapeType = info.internalName;
    m_params = info.defaultParams;

    createSliders(info.paramNames);

    if (m_rotateCheckbox) {
        m_rotateCheckbox->blockSignals(true);
        m_rotateCheckbox->setChecked(info.defaultRotate);
        m_rotateCheckbox->blockSignals(false);
        m_renderWidget->setRotationEnabled(info.defaultRotate);
    }

    // FIX: reset to the minimum before measuring the new shape's card, so
    // a long formula from the *previous* shape doesn't leave the panel
    // oversized after switching to a shape that doesn't need it.
    m_propsPanelWidth = kPropsPanelMinWidth;

    rebuildPropsCard();
    updatePropsPanelWidth();
    updateShape();
}

void GeoModeWidget::rebuildPropsCard() {
    if (m_propsCard) {
        m_propsPanelLayout->removeWidget(m_propsCard);
        m_propsCard->deleteLater();
        m_propsCard = nullptr;
    }
    m_propsCard = createPropsCard(m_shapeType, m_params, nullptr);
    if (!m_propsCard) return;

    m_propsCard->setParent(m_propertiesPanel);
    m_propsPanelLayout->insertWidget(0, m_propsCard);

    // Clicking ON a visible formula plays the walkthrough in the viewer.
    // (The formula glows on hover to advertise this.)
    connect(m_propsCard, &GeoCard::formulaClicked, this,
        [this](const QString& rowKey) {
            m_renderWidget->startFormulaAnimation(m_shapeType, rowKey);
        });
    // Hiding the formula (row click) stops a running walkthrough.
    connect(m_propsCard, &GeoCard::formulaActivated, this,
        [this](const QString& /*rowKey*/, bool visible) {
            if (!visible) m_renderWidget->stopFormulaAnimation();
        });
}
void GeoModeWidget::resizeEvent(QResizeEvent*) {
    if (!m_controlPanel || !m_backButton) return;

    int cpWidth = m_controlPanel->width();
    m_controlPanel->setGeometry(width() - cpWidth, 0, cpWidth, height());
    m_backButton->move((width() - m_backButton->width()) / 2, height() - 50);

    if (m_topBar) {
        m_topBar->move((width() - m_topBar->width()) / 2, 10);
        m_topBar->raise();
    }
    if (m_propertiesPanel) {
        // FIX: was hardcoded `260` here too — duplicated the constant
        // from the constructor and would silently override any dynamic
        // width set by updatePropsPanelWidth(). Now uses the single
        // stored m_propsPanelWidth.
        m_propertiesPanel->setGeometry(0, 0, m_propsPanelWidth, height());
        m_propertiesPanel->raise();
    }
}
void GeoModeWidget::setShape(const QString& type, const QMap<QString, double>& params) {
    m_shapeType = type;
    m_params = params;

    // Update combo box selection to match the shape type
    for (int i = 0; i < ALL_SHAPES_INFO().size(); ++i) {
        if (ALL_SHAPES_INFO()[i].internalName == type) {
            // Block signals to avoid triggering onShapeChanged recursively
            m_shapeSelector->blockSignals(true);
            m_shapeSelector->setCurrentIndex(i);
            m_shapeSelector->blockSignals(false);
            break;
        }
    }

    createSliders(params.keys());
    // Shared builder: this path used to duplicate the card construction
    // WITHOUT the formula-click connections, which silently broke the
    // walkthrough trigger when geometry mode was entered from a card.
    rebuildPropsCard();
    updatePropsPanelWidth();
    updateShape();
}
void GeoModeWidget::clearSliders()
{
    while (m_slidersLayout->rowCount() > 0)
    {
        m_slidersLayout->removeRow(0);
    }

    m_sliders.clear();
    m_labels.clear();
}
void GeoModeWidget::createSliders(const QStringList& paramNames) {
    // Remove existing slider widgets from layout and delete them
    clearSliders();
    for (const QString& name : paramNames) {
        double minVal, maxVal, defaultVal;
        getParamRange(m_shapeType, name, minVal, maxVal, defaultVal);
        double val = m_params.value(name, defaultVal);

        QLabel* label = new QLabel(name + " =");
        label->setFont(Theme::monoFont(9));
        label->setStyleSheet("color:#5a6b5f;background:#000000");

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
    getParamRange(m_shapeType, name, minVal, maxVal, dummy);
    double val = minVal + (s->value() / 100.0) * (maxVal - minVal);
    m_params[name] = val;
    if (m_labels.contains(name))
        m_labels[name]->setText(QString::number(val, 'g', 3));

    if (m_propsCard)
        m_propsCard->applyParams(m_params);

    // FIX (new): a slider drag can change a displayed value's digit count
    // (e.g. "4.19" -> "1204.2604"), which changes the row's natural
    // width. Re-check so the panel keeps growing to fit as values change,
    // not just at shape-switch time.
    updatePropsPanelWidth();

    updateShape();
}

void GeoModeWidget::updateShape() {
    m_renderWidget->setShape(m_shapeType, m_params);
    
}



void GeoModeWidget::onBack() {
    m_renderWidget->stopFormulaAnimation();
    m_renderWidget->clearMeshes();
    emit backClicked();
}

GeoModeWidget::~GeoModeWidget()
{
    if (m_propertiesPanel) m_propertiesPanel->deleteLater();
    if (m_propsCard) m_propsCard->deleteLater();
    if (m_propsPanelLayout) m_propsPanelLayout->deleteLater();
    m_propertiesPanel = nullptr;
    m_propsCard = nullptr;
    m_propsPanelLayout = nullptr;

    qDebug() << "~GeoModeWidget";
}