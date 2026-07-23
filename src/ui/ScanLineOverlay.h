#include <QPainter>
#include <QWidget>
#include "../constants/Theme.h"


// Transparent overlay widget sitting on top of MainWindow
class ScanlineOverlay : public QWidget {


protected:
    void paintEvent(QPaintEvent*) override {
        QImage img(size(), QImage::Format_ARGB32_Premultiplied);
       

        QPainter p(&img);
        for (int y = 0; y < height(); y += 3) {
            p.setPen(QColor(11, 13, 12, 35));
            p.drawLine(0, y, width(), y);
        }
        p.end();
        QColor m_blackReplaceColor = QColor(C_BG.toStdString().c_str());
        const QRgb replacement = m_blackReplaceColor.rgb();
        QSet<QRgb> targets = { qRgb(7,8,8), qRgb(8,9,9), qRgb(6,8,7), qRgb(7,8,7), qRgb(0,0,0)};
        for (int y = 0; y < img.height(); ++y) {
            QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
            for (int x = 0; x < img.width(); ++x) {
                QRgb pixel = line[x] & 0x00FFFFFF;
                if (targets.contains(pixel))
                    line[x] = 0xFF000000 | replacement;
            }
        }

        QPainter widgetPainter(this);
        widgetPainter.drawImage(0, 0, img);
    }
};