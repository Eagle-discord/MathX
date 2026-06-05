#include "GeoCard.h"
#include <QApplication>
#include <QClipboard>
#include <QMouseEvent>
#include "../constants/Theme.h"
#include <QFrame>
#include <QTimer>
#include <cmath>
#include "..\ui\MainWindow.h"


static QFont MF(int pt, int w = QFont::Normal) { return Theme::monoFont(pt, w); }

SliderRow::SliderRow(const QString& name, double init,
    double minV, double maxV, double step, QWidget* parent)
    : QWidget(parent), m_name(name), m_min(minV), m_max(maxV), m_step(step)
{
    setStyleSheet("background:transparent;");
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 2, 0, 2); row->setSpacing(8);

    m_nameLabel = new QLabel(name + " =");
    m_nameLabel->setFont(MF(9)); m_nameLabel->setFixedWidth(44);
    m_nameLabel->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::MUTED));
    row->addWidget(m_nameLabel);

    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(0, toTick(maxV));
    m_slider->setValue(toTick(init));
    m_slider->setStyleSheet(QString(R"(
        QSlider::groove:horizontal{height:3px;background:%1;border-radius:2px;}
        QSlider::handle:horizontal{background:%2;border:2px solid %2;width:14px;height:14px;margin:-6px 0;border-radius:7px;}
        QSlider::handle:horizontal:hover{background:%3;border-color:%3;width:16px;height:16px;margin:-7px 0;border-radius:8px;}
        QSlider::sub-page:horizontal{background:%2;border-radius:2px;}
    )").arg(Theme::DIM, Theme::ACCENT, Theme::TEXT));
    row->addWidget(m_slider, 1);

    m_valLabel = new QLabel(QString::number(init, 'g', 6));
    m_valLabel->setFont(MF(9, QFont::Bold)); m_valLabel->setFixedWidth(62);
    m_valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_valLabel->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::ACCENT));
    row->addWidget(m_valLabel);

    m_popup = new QLabel(this);
    m_popup->setFont(MF(9, QFont::Bold)); m_popup->setAlignment(Qt::AlignCenter);
    m_popup->setStyleSheet(QString(
        "background:%1;color:%2;border:1px solid %2;border-radius:6px;padding:2px 8px;"
    ).arg(Theme::SURFACE, Theme::ACCENT));
    m_popup->hide(); m_popup->setFixedHeight(22);

    connect(m_slider, &QSlider::valueChanged, this, &SliderRow::onSliderMoved);
    connect(m_slider, &QSlider::sliderPressed, this, [this] { showPopup(fromTick(m_slider->value())); });
    connect(m_slider, &QSlider::sliderReleased, this, [this] { hidePopup(); });
}

int SliderRow::toTick(double v) const { return (int)std::round((v - m_min) / m_step); }
double SliderRow::fromTick(int t) const { return m_min + t * m_step; }
double SliderRow::value() const { return fromTick(m_slider->value()); }
void SliderRow::setValue(double v) { m_slider->setValue(toTick(v)); }
void SliderRow::onSliderMoved(int tick) {
    double val = fromTick(tick);
    m_valLabel->setText(QString::number(val, 'g', 6));
    if (m_popup->isVisible()) showPopup(val);
    emit valueChanged(val);
}

QString ResultRow::displayText() const {
    QString line = QString("%1  %2").arg(m_rawKey, m_currentValue);
    if (m_formulaVisible && !m_formulaString.isEmpty())
        line += QString(" = %1").arg(m_formulaString);
    return line;
}

void SliderRow::showPopup(double val) {
    m_popup->setText(m_name + " = " + QString::number(val, 'g', 6));
    m_popup->adjustSize();
    int range = m_slider->maximum() - m_slider->minimum();
    int pos = range > 0 ? (int)((double)(m_slider->value() - m_slider->minimum()) / range * m_slider->width()) : 0;
    int px = m_slider->x() + pos - m_popup->width() / 2;
    px = std::max(0, std::min(px, width() - m_popup->width()));
    m_popup->move(px, -26); m_popup->show(); m_popup->raise();
}
void SliderRow::hidePopup() { m_popup->hide(); }
ResultRow::ResultRow(const QString& key, const QString& formula, QWidget* parent)
    : QWidget(parent), m_formulaString(formula), m_rawKey(key) {
    setStyleSheet("background:transparent;");
    setCursor(Qt::PointingHandCursor);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(0, 2, 0, 2);
    row->setSpacing(8);

    m_keyLbl = new QLabel(key);
    m_keyLbl->setFont(Theme::monoFont(10));
    m_keyLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::MUTED));
    m_keyLbl->setFixedWidth(140); 

    m_valLbl = new QLabel("—");
    m_valLbl->setFont(Theme::monoFont(9, QFont::Bold));
    m_valLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::INFO));

    m_formulaInline = new QLabel("");
    m_formulaInline->setFont(Theme::monoFont(10));
    m_formulaInline->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::FTEXT));
    m_formulaInline->hide();

    row->addWidget(m_keyLbl);
    row->addWidget(m_valLbl, 1);
    row->addWidget(m_formulaInline);
}
void ResultRow::setValue(const QString& v) {
    m_currentValue = v;                                                                                           
    m_valLbl->setText(v);
}
QString ResultRow::value() const {
    return m_currentValue;
}
void ResultRow::enterEvent(QEnterEvent*) {
    if (m_copied) return;
    // Show " Copy" hint on the value label
    m_valLbl->setText(m_currentValue +
        QString("<span style='color:%1;font-size:10pt;'> &nbsp;&#10064; Right click to copy</span>")
        .arg(Theme::MUTED));
}

void ResultRow::leaveEvent(QEvent*) {
    if (m_copied) return;
    m_valLbl->setText(m_currentValue);
}

void ResultRow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::RightButton) {
        // Right‑click copies the value
        QApplication::clipboard()->setText(m_currentValue);
        m_copied = true;
        m_valLbl->setText(QString("<span style='color:%1;'>Copied!</span>").arg(Theme::ACCENT));
        setStyleSheet(QString("background:%1;border-radius:4px;").arg(Theme::HOVER));
        QTimer::singleShot(1200, this, [this]() {
            m_copied = false;
            m_valLbl->setText(m_currentValue);
            if (!m_formulaVisible)
                setStyleSheet("background:transparent;");
            });
    }
    else if (e->button() == Qt::LeftButton) {
        toggleFormula();
    }
    QWidget::mousePressEvent(e);
}


void ResultRow::mouseDoubleClickEvent(QMouseEvent* e) {

    QWidget::mouseDoubleClickEvent(e);
}

void ResultRow::toggleFormula() {
    m_formulaVisible = !m_formulaVisible;
    if (m_formulaVisible) {
        m_formulaInline->setText(" = " + m_formulaString);
        m_formulaInline->show();
    }
    else {
        m_formulaInline->hide();
    }
}


GeoCard::GeoCard(QWidget* parent) : QWidget(parent) { buildFrame(); }
void GeoCard::buildFrame() {
    setObjectName("geoCard");
    setStyleSheet(QString("QWidget#geoCard{background:%1;border:1px solid %2;border-radius:10px;}").arg(Theme::SURFACE, Theme::BORDER));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 12, 14, 14); outer->setSpacing(6);

    // Copy card button — sits in the top-right corner of the card
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
    ).arg(Theme::MUTED, Theme::ACCENT));
    topL->addWidget(m_copyBtn);

    m_toggleBtn = new QPushButton("Show Shape Projection");
    m_toggleBtn->setFont(Theme::monoFont(9));
    m_toggleBtn->setFlat(true);
    m_toggleBtn->setCursor(Qt::PointingHandCursor);
    m_toggleBtn->setStyleSheet(QString(
        "QPushButton{background:none;border:1px solid %1;color:%1;"
        "padding:1px 8px;border-radius:4px;}"
        "QPushButton:hover{border-color:%2;color:%2;}"
    ).arg(Theme::MUTED, Theme::ACCENT));

    topL->addWidget(m_toggleBtn);
    outer->addWidget(topRow);

    connect(m_copyBtn, &QPushButton::clicked, this, [this]() {
        QString text = buildCopyText();
        QApplication::clipboard()->setText(text);
        m_copyBtn->setText("Copied!");
        m_copyBtn->setStyleSheet(QString(
            "QPushButton{background:%1;border:1px solid %1;color:#000;"
            "padding:1px 8px;border-radius:4px;}"
        ).arg(Theme::ACCENT));
        QTimer::singleShot(1500, this, [this]() {
            m_copyBtn->setText("Copy Card");
            m_copyBtn->setStyleSheet(QString(
                "QPushButton{background:none;border:1px solid %1;color:%1;"
                "padding:1px 8px;border-radius:4px;}"
                "QPushButton:hover{border-color:%2;color:%2;}"
            ).arg(Theme::MUTED, Theme::ACCENT));
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
    auto* r = new SliderRow(name, val, mn, mx, step, m_body);
    connect(r, &SliderRow::valueChanged, this, &GeoCard::recompute);
    m_layout->addWidget(r);
    m_sliders[name] = r;
    m_sliderOrder.append(name);
    return r;
}
ResultRow* GeoCard::addResult(const QString& key, const QString& formula) {
    auto* r = new ResultRow(key, formula, m_body);
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
    l->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::ACCENT));
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