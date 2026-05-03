#include "GeoCard.h"
#include "../theme/Theme.h"
#include <QFrame>
#include <QTimer>
#include <cmath>
#include "..\ui\MainWindow.h"
#include "..\ui\OutputArea.h"


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
    : QWidget(parent)
{
    setStyleSheet("background:transparent;");
    setCursor(Qt::PointingHandCursor);
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(0, 0, 0, 0); col->setSpacing(0);

    m_formulaLbl = new QLabel("\xce\xa6 = " + formula);
    m_formulaLbl->setFont(MF(8));
    m_formulaLbl->setStyleSheet(QString(
        "color:%1;background:#1e2a18;border-left:2px solid %1;padding:2px 8px;"
    ).arg(Theme::FTEXT));
    // Use setVisible(false) + Fixed/Ignored size policy so the hidden label
    // takes zero vertical space in the layout until toggled visible.
    m_formulaLbl->setVisible(false);
    m_formulaLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    col->addWidget(m_formulaLbl);

    auto* row = new QWidget; row->setStyleSheet("background:transparent;");
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 1, 0, 1); hl->setSpacing(0);

    m_keyLbl = new QLabel(key); m_keyLbl->setFont(MF(9));
    m_keyLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::MUTED));
    m_keyLbl->setFixedWidth(140);

    m_valLbl = new QLabel("\xe2\x80\x94"); m_valLbl->setFont(MF(9, QFont::Bold));
    m_valLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::INFO));

    hl->addWidget(m_keyLbl); hl->addWidget(m_valLbl, 1);
    col->addWidget(row);
}
void ResultRow::setValue(const QString& v) { m_valLbl->setText(v); }
QString ResultRow::value() const { return m_valLbl->text(); }
void ResultRow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) toggleFormula();
    QWidget::mousePressEvent(e);
}
void ResultRow::toggleFormula() {
    m_formulaVisible = !m_formulaVisible;
    m_formulaLbl->setVisible(m_formulaVisible);
    setStyleSheet(m_formulaVisible
        ? QString("background:%1;border-radius:4px;").arg(Theme::HOVER)
        : "background:transparent;");
}

GeoCard::GeoCard(QWidget* parent) : QWidget(parent) { buildFrame(); }
void GeoCard::buildFrame() {
    setObjectName("geoCard");
    setStyleSheet(QString("QWidget#geoCard{background:%1;border:1px solid %2;border-radius:10px;}").arg(Theme::SURFACE, Theme::BORDER));
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(14, 12, 14, 14); outer->setSpacing(6);
    m_body = new QWidget(this); m_body->setStyleSheet("background:transparent;");
    m_layout = new QVBoxLayout(m_body);
    m_layout->setContentsMargins(0, 0, 0, 0); m_layout->setSpacing(2);
    outer->addWidget(m_body);
}
SliderRow* GeoCard::addSlider(const QString& name, double val, double mn, double mx, double step) {
    auto* r = new SliderRow(name, val, mn, mx, step, m_body);
    connect(r, &SliderRow::valueChanged, this, &GeoCard::recompute);
    m_layout->addWidget(r); m_sliders[name] = r; return r;
}
ResultRow* GeoCard::addResult(const QString& key, const QString& formula) {
    auto* r = new ResultRow(key, formula, m_body);
    m_layout->addWidget(r); m_rows[key] = r;

    return r;
}

QFrame* geoDiv() {
    auto* d = new QFrame; d->setFrameShape(QFrame::HLine); d->setFixedHeight(1);
    d->setStyleSheet(QString("background:%1;border:none;").arg(Theme::DIM));
    return d;
}
QLabel* geoTitle(const QString& text) {
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