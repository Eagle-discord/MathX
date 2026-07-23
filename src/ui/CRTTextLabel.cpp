#include "CRTTextLabel.h"
//#include "OutputArea.h"

#include <QPainter>
#include <QRadialGradient>
#include <QFontDatabase>
#include <QFontDialog>
#include <QFontMetrics>
#include <QMouseEvent>
#include <cstdlib>

CRTTextLabel::CRTTextLabel(const QString& text, QWidget* parent)
    : QWidget(parent), m_text(text)
{
    m_font = QFont("Syne", 20, QFont::ExtraBold);
    m_subtitleFont = QFont("Orbitron", 9, QFont::Normal);




   // m_flickerTimer = new QTimer(this);
   // connect(m_flickerTimer, &QTimer::timeout, this, &CRTTextLabel::onFlickerTick);
   // m_flickerTimer->start(80);
}

// -- Setters ----------------------------------------------------------------

void CRTTextLabel::setText(const QString& text) {
    if (m_text == text) return;
    m_text = text;
    update();
    emit textChanged(m_text);
}

void CRTTextLabel::setDisplayFont(const QFont& font) {
    m_font = font;
    m_subtitleFont.setFamily(font.family()); // keep subtitle family in sync
    updateGeometry();
    update();
    emit fontChanged(m_font);
}

void CRTTextLabel::loadFontFromFile(const QString& ttfPath, int pointSize) {
    int id = QFontDatabase::addApplicationFont(ttfPath);
    if (id == -1) {
        qWarning() << "CRTTextLabel: failed to load font from" << ttfPath;
        return;
    }
    QString family = QFontDatabase::applicationFontFamilies(id).value(0);
    QFont f(family, pointSize, QFont::Bold);
    setDisplayFont(f);
}

void CRTTextLabel::openFontPicker() {
    bool ok = false;
    QFont chosen = QFontDialog::getFont(&ok, m_font, this, "Choose CRT Font");
    if (ok) setDisplayFont(chosen);
}

void CRTTextLabel::setGlowColor(const QColor& color) {
    m_glowColor = color;
    update();
}

void CRTTextLabel::setGlowLayers(int layers) {
    m_glowLayers = qBound(0, layers, 6);
    update();
}

void CRTTextLabel::setCrtEnabled(bool enabled) {
    m_crtEnabled = enabled;
    update();
}

void CRTTextLabel::setFlickerEnabled(bool enabled) {
    m_flickerEnabled = enabled;
    if (enabled) m_flickerTimer->start();
    else         m_flickerTimer->stop();
    m_flickerAlpha = 255;
    update();
}

void CRTTextLabel::setScanlineOpacity(int opacity) {
    m_scanlineOpacity = qBound(0, opacity, 255);
    update();
}

void CRTTextLabel::setFlickerInterval(int ms) {
    m_flickerTimer->setInterval(ms);
}

void CRTTextLabel::setSubtitle(const QString& subtitle, int pointSize) {
    m_subtitle = subtitle;
    m_subtitleSize = pointSize;
    m_subtitleFont = QFont(m_font.family(), pointSize, QFont::Normal);
    updateGeometry();
    update();
}

// -- Size hint --------------------------------------------------------------

QSize CRTTextLabel::sizeHint() const {
    QFontMetrics fm(m_font);
    QFontMetrics sfm(m_subtitleFont);

    // Take the wider of title or subtitle
    int w = qMax(fm.horizontalAdvance(m_text),
        sfm.horizontalAdvance(m_subtitle) + m_subtitle.size() * 3) + 20;

    int h = fm.height()
        + (m_subtitle.isEmpty() ? 0 : sfm.height() + 4)
        + 20;

    return QSize(w, h);
}

// -- Paint ------------------------------------------------------------------

void CRTTextLabel::paintEvent(QPaintEvent*) {
    QImage img(size(), QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    QPainter p(&img);

    QFontMetrics fm(m_font);
    int textH = fm.height();
    int totalH = textH + (m_subtitle.isEmpty() ? 0 : QFontMetrics(m_subtitleFont).height() + 4);
    int topY = qMax(0, (height() - totalH) / 2);
    QRect textRect(0, topY, width(), textH);

    if (m_glowLayers > 0) drawGlow(p, textRect);
    drawText(p, textRect);
    if (!m_subtitle.isEmpty()) drawSubtitle(p, textRect);
    if (m_crtEnabled) {
        drawScanlines(p, img);
        drawVignette(p, img);
    }
    p.end();
    static const QSet<QRgb> targets = {
      qRgb(7,8,8), qRgb(8,9,9), qRgb(6,8,7), qRgb(7,8,7), qRgb(0,0,0)
    };
    // Strip before CRT effects touch the image
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            QColor current(line[x]);
            int tolerance = 30;
            for (QColor targetColor : targets){
                if (qAbs(current.red() - targetColor.red()) <= tolerance &&
                    qAbs(current.green() - targetColor.green()) <= tolerance &&
                    qAbs(current.blue() - targetColor.blue()) <= tolerance) {
                    line[x] = background;
                    break;
                }
            }
        }
    
    }
    
    QPainter widgetPainter(this);
    
    widgetPainter.drawImage(0, 0, img);
}
void CRTTextLabel::drawGlow(QPainter& p, const QRect& r) {
    int alpha = m_flickerEnabled ? m_flickerAlpha : 255;
    p.setFont(m_font);
    for (int i = m_glowLayers; i >= 1; --i) {
        QColor c = m_glowColor;
        c.setAlpha(qBound(0, (50 / i), 255) * alpha / 255);
        p.setPen(c);
        // Expand rect slightly per layer for bloom spread
        QRect bloomed = r.adjusted(-i * 2, -i, i * 2, i);
        p.drawText(bloomed, Qt::AlignLeft | Qt::AlignVCenter, m_text);
    }
}

void CRTTextLabel::drawText(QPainter& p, const QRect& r) {
    int alpha = m_flickerEnabled ? m_flickerAlpha : 255;
    p.setFont(m_font);
    QColor c = m_glowColor;
    c.setAlpha(alpha);
    p.setPen(c);
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

void CRTTextLabel::drawSubtitle(QPainter& p, const QRect& mainRect) {
    int alpha = m_flickerEnabled ? m_flickerAlpha : 255;
    QColor c = m_glowColor;
    c.setAlpha(alpha * 0.55); // dimmer than main text
    p.setPen(c);
    p.setFont(m_subtitleFont);
    QRect subRect(mainRect.left(), mainRect.bottom() + 4, mainRect.width(),
        QFontMetrics(m_subtitleFont).height());
    // Letter-spacing simulation via manual draw
    int x = subRect.left();
    int spacing = 3;
    for (QChar ch : m_subtitle) {
        p.drawText(x, subRect.top(), QFontMetrics(m_subtitleFont).horizontalAdvance(ch) + spacing,
            subRect.height(), Qt::AlignLeft | Qt::AlignVCenter, ch);
        x += QFontMetrics(m_subtitleFont).horizontalAdvance(ch) + spacing;
    }
}

void CRTTextLabel::drawScanlines(QPainter& p, QImage& img) {
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    p.setPen(QColor(0, 0, 0, m_scanlineOpacity));
    for (int y = 0; y < height(); y += 3)
        p.drawLine(0, y, width(), y);
    p.end();

    stripTargetColors(img);

    // reopen painter for next draw call
    p.begin(&img);
}

void CRTTextLabel::drawVignette(QPainter& p, QImage& img) {
    QRadialGradient vignette(rect().center(), width() * 0.65);
    vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
    vignette.setColorAt(1.0, QColor(0, 0, 0, 100));
    p.fillRect(rect(), vignette);
    p.end();

    stripTargetColors(img);

    p.begin(&img);
}
void CRTTextLabel::stripTargetColors(QImage& img) {
    static const QSet<QRgb> targets = {
        qRgb(7,8,8), qRgb(8,9,9), qRgb(6,8,7), qRgb(7,8,7), qRgb(0,0,0)
    };
    const QRgb background = QColor("#0b0d0c").rgb();
    for (int y = 0; y < img.height(); ++y) {
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < img.width(); ++x)
            if (targets.contains(line[x] & 0x00FFFFFF))
                line[x] = 0xFF000000 | background;
    }
}

// -- Slots ------------------------------------------------------------------

void CRTTextLabel::onFlickerTick() {
    // Gentle organic flicker - mostly steady with occasional dips
    int roll = rand() % 100;
    if (roll < 5)        m_flickerAlpha = 180 + rand() % 40; // rare dim
    else if (roll < 15)  m_flickerAlpha = 220 + rand() % 35; // occasional
    else                 m_flickerAlpha = 245 + rand() % 10; // mostly steady
    update();
}

void CRTTextLabel::mouseDoubleClickEvent(QMouseEvent* event) {
    Q_UNUSED(event)
        openFontPicker(); // double-click the label to change font
}