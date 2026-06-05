
#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QPainter>
#include <QRandomGenerator>

class CRTLogoWidget : public QWidget {
    Q_OBJECT
public:
    CRTLogoWidget(QWidget* parent = nullptr) : QWidget(parent) {
        // Flicker timer
        QTimer* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() {
            flickerAlpha = 200 + (QRandomGenerator::global()->bounded(55 % 55)); // subtle brightness flicker
            update();
            });
        timer->start(8); // ~12fps flicker feels organic
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        
        // Draw the text
        QFont font("Orbitron", 20, QFont::Bold);
        p.setFont(font);
        p.setPen(QColor(0, 255, 65, flickerAlpha));
        p.drawText(rect(), Qt::AlignLeft | Qt::AlignVCenter, "MATHX");

        // Draw scanlines over the top
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (int y = 0; y < height(); y += 3) {
            p.setPen(QColor(0, 0, 0, 60)); // semi-transparent black lines
            p.drawLine(0, y, width(), y);
       }

        // Vignette: darken edges
        QRadialGradient vignette(rect().center(), width() * 0.6);
        vignette.setColorAt(0.0, QColor(0, 0, 0, 0));
        vignette.setColorAt(1.0, QColor(0, 0, 0, 120));
        p.fillRect(rect(), vignette);
    }

private:
    int flickerAlpha = 255;
};