#include "GeoCard.h"
#include <QApplication>
#include <QClipboard>
#include <QMouseEvent>
#include "../constants/Theme.h"
#include <QFrame>
#include <QTimer>
#include <cmath>
#include "../ui/MainWindow.h"
#include "SliderRow.h"
#include "ResultRow.h"

static QFont MF(int pt, int w = QFont::Normal) { return Theme::monoFont(pt, w); }



GeoCard::GeoCard(QWidget* parent, Mode mode) : QWidget(parent), m_mode(mode) {
    // FIX: constructor previously never assigned `mode` to `m_mode` - it
    // only used the local parameter to pick which frame to build, so the
    // member (default-initialized to Mode::Full in the header) stayed
    // Full forever. That silently broke param()/applyParams()/addSlider()
    // for every PropertiesOnly card, since they all branch on m_mode.
    if (mode == Mode::Full)
        buildFrame();
    else
        buildPropertiesFrame();
}

void GeoCard::buildPropertiesFrame() {
    setObjectName("geoCard");
    setStyleSheet("background: transparent; border: none;");
    m_body = new QWidget(this);
    m_body->setStyleSheet("background: transparent;");
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(m_body);
    m_layout = new QVBoxLayout(m_body);
    m_layout->setContentsMargins(8, 8, 8, 8);
    m_layout->setSpacing(2);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void GeoCard::buildFrame() {
    setObjectName("geoCard");
    setStyleSheet(QString("QWidget#geoCard{background:%1;border:1px solid %2;border-radius:10px;}").arg(Theme::SURFACE, Theme::BORDER));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 12, 14, 14); outer->setSpacing(6);

    // Copy card button - sits in the top-right corner of the card
    auto* topRow = new QWidget(this); topRow->setStyleSheet("background:transparent;");
    auto* topL = new QHBoxLayout(topRow);
    topL->setContentsMargins(0, 0, 0, 0); topL->setSpacing(0);
    topL->addStretch();
    m_copyBtn = new QPushButton("Copy Card");
    m_copyBtn->setFont(Theme::monoFont(9));
    m_copyBtn->setFlat(true);
    m_copyBtn->setCursor(Qt::PointingHandCursor);
    m_copyBtn->setStyleSheet(QString(
        "QPushButton{background:none;border:1px solid %1;color:%1;"
        "padding:1px 8px;border-radius:4px;}"
        "QPushButton:hover{border-color:%2;color:%2;}"
    ).arg(Theme::MUTED, Theme::ACCENT()));
    topL->addWidget(m_copyBtn);

    m_toggleBtn = new QPushButton("Show Shape Projection");
    m_toggleBtn->setFont(Theme::monoFont(9));
    m_toggleBtn->setFlat(true);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setStyleSheet(QString(
        "QPushButton{background:none;border:1px solid %1;color:%1;"
        "padding:1px 8px;border-radius:4px;}"
        "QPushButton:hover{border-color:%2;color:%2;}"
    ).arg(Theme::MUTED, Theme::ACCENT()));

    topL->addWidget(m_toggleBtn);
    outer->addWidget(topRow);

    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        QString text = buildCopyText();
        QApplication::clipboard()->setText(text);
        m_copyBtn->setText("Copied!");
        m_copyBtn->setStyleSheet(QString(
            "QPushButton{background:%1;border:1px solid %1;color:#000;"
            "padding:1px 8px;border-radius:4px;}"
        ).arg(Theme::ACCENT()));
        QTimer::singleShot(1500, this, [this]() {
            m_copyBtn->setText("Copy Card");
            m_copyBtn->setStyleSheet(QString(
                "QPushButton{background:none;border:1px solid %1;color:%1;"
                "padding:1px 8px;border-radius:4px;}"
                "QPushButton:hover{border-color:%2;color:%2;}"
            ).arg(Theme::MUTED, Theme::ACCENT()));
            });
        });
    connect(m_toggleBtn, &QPushButton::clicked, this, [this]() {
        QMap<QString, double> params;
        for (auto it = m_sliders.begin(); it != m_sliders.end(); ++it) {
            params[it.key()] = it.value()->value();
        }
        emit showShapeProjection(m_shapeType, params);
        });
    m_body = new QWidget(this); m_body->setStyleSheet("background:transparent;");
    m_layout = new QVBoxLayout(m_body);
    m_layout->setContentsMargins(0, 0, 0, 0); m_layout->setSpacing(2);
    outer->addWidget(m_body);
}
SliderRow* GeoCard::addSlider(const QString& name, double val, double mn, double mx, double step) {
    if (m_mode == Mode::PropertiesOnly) {
        // Store default value but don't create a slider widget
        m_paramValues[name] = val;
        return nullptr;
    }
    auto* r = new SliderRow(name, val, mn, mx, step, m_body);
    connect(r, &SliderRow::valueChanged, this, &GeoCard::recompute);
    m_layout->addWidget(r);
    m_sliders[name] = r;
    m_sliderOrder.append(name);
    return r;
}
ResultRow* GeoCard::addResult(const QString& key, const QString& formula) {
    auto* r = new ResultRow(key, formula, m_body);
    connect(r, &ResultRow::formulaToggled, this, &GeoCard::formulaActivated);
    connect(r, &ResultRow::formulaClicked, this, &GeoCard::formulaClicked);
    m_layout->addWidget(r);
    m_rows[key] = r;
    m_rowOrder.append(key);
    return r;
}

// Builds the formatted copy text in the agreed format:
//   1. Shape title
//   2. Slider values (parameters)
//   3. Result key/value pairs
QString GeoCard::buildCopyText() const {
    QString out;

    // 1. Title
    out += m_title.isEmpty() ? "Shape" : m_title;
    out += "\n";

    // 2. Slider values
    for (const QString& name : m_sliderOrder) {
        if (!m_sliders.contains(name)) continue;
        out += QString("  %1 = %2\n")
            .arg(name)
            .arg(m_sliders[name]->value(), 0, 'g', 10);
    }

    out += "\n";

    // 3. Result rows with optional formulas
    for (const QString& key : m_rowOrder) {
        if (!m_rows.contains(key)) continue;
        out += "  " + m_rows[key]->displayText() + "\n";

    }

    return out.trimmed();
}

QFrame* geoDiv() {
    auto* d = new QFrame; d->setFrameShape(QFrame::HLine); d->setFixedHeight(1);
    d->setStyleSheet(QString("background:%1;border:none;").arg(Theme::DIM));
    return d;
}
QLabel* geoTitle(const QString& text, GeoCard* card) {
    if (card) card->setTitle(text);
    auto* l = new QLabel(text);
    l->setFont(Theme::monoFont(10, QFont::Bold));
    l->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::ACCENT()));
    return l;
}
QLabel* geoHint() {
    auto* l = new QLabel("click any value to see its formula");
    l->setFont(Theme::monoFont(7));
    l->setStyleSheet(QString("color:%1;background:transparent;font-style:italic;").arg(Theme::MUTED));
    return l;
}
double geoAutoMax(double v) { return std::max(v * 10.0, 50.0); }
double geoAutoStep(double v) {
    if (v > 200) return 5.0;
    if (v > 50)  return 1.0;
    if (v > 5)   return 0.5;
    return 0.1;
}