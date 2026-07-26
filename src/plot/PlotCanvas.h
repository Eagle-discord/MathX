#pragma once
#include <QPoint>
#include <QWidget>
#include <QFont>
#include "Viewport.h"
#include <QTimer>
#include <QElapsedTimer>

class PlotCanvas : public QWidget {
	Q_OBJECT;
public:
	explicit PlotCanvas(QWidget* parent = nullptr);
	void resetView();


protected:
	void paintEvent(QPaintEvent* paintEvent) override;

	

	
	
	void resizeEvent(QResizeEvent* resizeEvent) override;
	void mousePressEvent(QMouseEvent* mouseEvent) override;
	void mouseMoveEvent(QMouseEvent* mouseEvent) override;
	void mouseReleaseEvent(QMouseEvent* mouseEvent) override;
	void wheelEvent(QWheelEvent* wheelEvent) override;

private:
	void drawGrid( QPainter& p);
	void drawAxes( QPainter& p);
	void drawSymbols( QPainter& p);
	void advanceFade();
	void drawGridLines( QPainter& p, double step, const QPen& pen);
	void drawTickLabels(QPainter& p);
	Viewport m_vp;
	QFont m_symbolFont;
	QFont m_numberFont;
	QPointF m_lastMousePos;
	bool m_dragging = false;

	float m_xPresence = 1.f;   // gates x / x′  (fades with the horizontal axis)
	float m_yPresence = 1.f;   // gates y / y′  (fades with the vertical axis)
	float m_presenceStep = 0.05f;
	QTimer m_fadeTimer;
	QElapsedTimer m_fadeClock;
	bool faded = false;
	float m_fadeStep = 0.01;

	// Guards the one-time viewport reset on the first resize. (The old
	// m_symbolsPlaced flag and the m_pos* label anchors are gone - axis labels
	// are now computed live from the view in drawSymbols, edge-pinned.)
	int resizeCounter = 0;
};