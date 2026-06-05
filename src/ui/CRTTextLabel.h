#pragma once

#include <QWidget>
#include <QFont>
#include <QColor>
#include <QTimer>
#include <QFontDialog>
#include <QPushButton>
#include "..\constants\Theme.h"
#include <QRgb>

class CRTTextLabel : public QWidget {
    Q_OBJECT

        Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
        Q_PROPERTY(QColor glowColor READ glowColor WRITE setGlowColor)
        Q_PROPERTY(bool crtEnabled READ crtEnabled WRITE setCrtEnabled)
        Q_PROPERTY(bool flickerEnabled READ flickerEnabled WRITE setFlickerEnabled)
        Q_PROPERTY(int scanlineOpacity READ scanlineOpacity WRITE setScanlineOpacity)
        Q_PROPERTY(int glowLayers READ glowLayers WRITE setGlowLayers)

public:
    explicit CRTTextLabel(const QString& text = "", QWidget* parent = nullptr);

    // Text
    QString text() const { return m_text; }
    void setText(const QString& text);

    // Font
    QFont displayFont() const { return m_font; }
    void setDisplayFont(const QFont& font);
    void openFontPicker();              // opens QFontDialog
    void loadFontFromFile(const QString& ttfPath, int pointSize = 28);

    // Glow
    QColor glowColor() const { return m_glowColor; }
    void setGlowColor(const QColor& color);
    int glowLayers() const { return m_glowLayers; }
    void setGlowLayers(int layers);     // 0 = no glow, 1–5 recommended

    // CRT
    bool crtEnabled() const { return m_crtEnabled; }
    void setCrtEnabled(bool enabled);
    bool flickerEnabled() const { return m_flickerEnabled; }
    void setFlickerEnabled(bool enabled);
    int scanlineOpacity() const { return m_scanlineOpacity; }
    void setScanlineOpacity(int opacity); // 0–255, recommended 30–60
    void setFlickerInterval(int ms);      // default 80ms

    // Subtitle (e.g. "UNLIMITED CALCULATOR")
    void setSubtitle(const QString& subtitle, int pointSize = 9);

    QSize sizeHint() const override;

signals:
    void textChanged(const QString& text);
    void fontChanged(const QFont& font);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override; // double-click = font picker

private slots:
    void onFlickerTick();

private:
    void stripTargetColors(QImage& img);
    void drawGlow(QPainter& p, const QRect& r);
    void drawText(QPainter& p, const QRect& r);
    void drawScanlines(QPainter& p, QImage& img);
    void drawVignette(QPainter& p, QImage& img);
    void drawSubtitle(QPainter& p, const QRect& r);
    QRgb background = QColor("#0b0d0c").rgb();
    QString  m_text;
    QString  m_subtitle;
    int      m_subtitleSize = 9;
    QFont    m_font;
    QFont    m_subtitleFont;
    QColor   m_glowColor{ 0, 255, 65 };
    QColor     m_blackReplaceColor = QColor(C_BG.toStdString().c_str());
    int      m_glowLayers = 3;
    bool     m_crtEnabled = true;
    bool     m_flickerEnabled = true;
    int      m_scanlineOpacity = 40;
    int      m_flickerAlpha = 255;
    QTimer* m_flickerTimer = nullptr;
};