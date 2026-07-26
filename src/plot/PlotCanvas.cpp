#include "PlotCanvas.h"
#include <qevent.h>
#include <QPen>
#include <QPainter>
#include <QFontMetricsF>
#include <algorithm>   // std::clamp
#include <cmath>       // std::round, std::ceil
#include "../ui/NumberFormat.h"


PlotCanvas::PlotCanvas(QWidget* parent)
{
	m_symbolFont = QFont("CMU Serif", 25);
	m_symbolFont.setItalic(true);
	m_numberFont = QFont("Noto Sans", 15);

	setParent(parent);
	setAttribute(Qt::WA_OpaquePaintEvent, true);

	m_fadeTimer.setInterval(16);
	connect(&m_fadeTimer, &QTimer::timeout, this, &PlotCanvas::advanceFade);
	m_fadeTimer.start();
}

void PlotCanvas::advanceFade() {

	// Axis straddle tests. Mind the pairing: the x-labels mark the HORIZONTAL
// axis, so they're gated by the y-range; y-labels by the x-range.
	const bool hAxisVisible = m_vp.yMin() <= 0 && m_vp.yMax() >= 0;   // → m_xPresence
	const bool vAxisVisible = m_vp.xMin() <= 0 && m_vp.xMax() >= 0;   // → m_yPresence

	const float xTarget = hAxisVisible ? 1.f : 0.f;
	const float yTarget = vAxisVisible ? 1.f : 0.f;
	m_xPresence = std::clamp(m_xPresence + (xTarget > m_xPresence ? m_presenceStep : -m_presenceStep), 0.f, 1.f);
	m_yPresence = std::clamp(m_yPresence + (yTarget > m_yPresence ? m_presenceStep : -m_presenceStep), 0.f, 1.f);
	update();
}

void PlotCanvas::resizeEvent(QResizeEvent* resizeEvent) {
	const QSize size = resizeEvent->size();
	m_vp.setViewportSize(size.width(), size.height());
	if (resizeCounter == 0) {
		resetView();
	}
	resizeCounter++;
	QWidget::resizeEvent(resizeEvent);
}

void PlotCanvas::mousePressEvent(QMouseEvent* mouseEvent)
{
	if (mouseEvent->button() == Qt::RightButton || mouseEvent->button() == Qt::LeftButton) {
		m_dragging = true; 
		m_lastMousePos = mouseEvent->pos();
		setCursor(Qt::ClosedHandCursor);
		mouseEvent->accept();
	}
	QWidget::mousePressEvent(mouseEvent);
	
}

void PlotCanvas::mouseMoveEvent(QMouseEvent* mouseEvent)
{
	if (m_dragging) {
		QPointF delta = mouseEvent->position() - m_lastMousePos;
		m_lastMousePos = mouseEvent->position();
		m_vp.panByPixels(
			delta.x() ,
			delta.y()
		);
		update();
		mouseEvent->accept();
	}
	QWidget::mouseMoveEvent(mouseEvent);
}

void PlotCanvas::mouseReleaseEvent(QMouseEvent* mouseEvent)
{
	if (m_dragging) {
		m_dragging = false;
		unsetCursor();
		mouseEvent->accept();
	}
	QWidget::mouseReleaseEvent(mouseEvent);
}

void PlotCanvas::wheelEvent(QWheelEvent* wheelEvent)
{
	QPoint delta = wheelEvent->angleDelta();
	// delta.y() returns 120 for a standard 15 degree mouse, we can divide it with 120 to get the number of notches (how far the mouse moved)
	double factor = std::pow(1.1, delta.y() / 120.0);
	m_vp.zoomAt(wheelEvent->position().x(), wheelEvent->position().y(), factor);
	update();
	wheelEvent->accept();
	QWidget::wheelEvent(wheelEvent);
}

void PlotCanvas::resetView() {
	// Axis labels are now computed live from the current view in drawSymbols
	// (edge-pinned), so there's nothing to bake here - reset just recentres.
	m_vp.reset();
	update();
}

void PlotCanvas::paintEvent(QPaintEvent* paintEvent) {
	QPainter* p = new QPainter(this);

	p->fillRect(this->rect(), Qt::black);
	drawGrid(*p);
	drawAxes(*p);
	drawSymbols(*p);
	drawTickLabels(*p);


	p->end();
	delete p;
	QWidget::paintEvent(paintEvent);
}
void PlotCanvas::drawGridLines( QPainter& p, double step, const QPen& pen) {
	double xMin = m_vp.xMin();
	double xMax = m_vp.xMax();
	double yMin = m_vp.yMin();
	double yMax = m_vp.yMax();
/*	if (xMin > 0 || yMin < 0) qDebug("Quadrant two is not visible");
	if (yMin > 0 || xMin < 0) qDebug("Quadrant three is not visible");
	if (xMax < 0 || yMax > 0) qDebug("Quadrant one is not visible");
	if (xMin < 0 || xMax < 0) qDebug("Quadrant four is not visible");*/
	//QString out = QString("xMin: %1, yMin: %3, xMax: %2,  yMax:%4").arg(xMin).arg(xMax).arg(yMin).arg(yMax);
	//qDebug() << out;
	double xFirst = std::ceil(xMin / step) * step;
	p.setPen(pen);
	for (int i = 0; ; ++i) {
		double x = xFirst + i * step;

		if (x > xMax) break;
		double sx = m_vp.worldToScreen(x, 0).x();
		sx = std::round(sx);
		p.drawLine(QPointF(sx, 0), QPointF(sx, height()));

	}

	double yFirst = std::ceil(yMin / step) * step;
	for (int i = 0; ; ++i) {
		double y = yFirst + i * step;

		if (y > yMax) break;
		double sy = m_vp.worldToScreen(0, y).y();
		sy = std::round(sy);
		p.drawLine(QPointF(0, sy), QPointF(width(), sy));

	}
}

void PlotCanvas::drawGrid(QPainter& p) {
	double step = m_vp.gridStep();
	double minorGridStep = m_vp.minorGridStep();
	QPen pen("#1e211f");
	drawGridLines(p, minorGridStep, pen);
	pen.setColor("#4b524e");
	drawGridLines(p, step, pen);
}

void PlotCanvas::drawAxes( QPainter& p)
{
	QPen pen("#FFFFFF");
	pen.setWidth(2);
	p.setPen(pen);
	
	QPointF center = m_vp.worldToScreen(0.0, 0.0);
	//draw a line from the app's left boundary and the world's y center, to the app's right boundary and the world's center, to get the X axis
	p.drawLine(
		QPointF(0, std::round(center.y())),
		QPointF(width(), std::round(center.y()))
	);
	
	// now for the y axis
	// draw a line (world center, app's top boundary

	p.drawLine(
		QPointF(std::round(center.x()), 0),
		QPointF(std::round(center.x()), height())

	);
}

void PlotCanvas::drawSymbols(QPainter& p) {
	p.setFont(m_symbolFont);
	QPen pen("#FFFFFF"); // White
	p.setPen(pen);
	p.setRenderHint(QPainter::TextAntialiasing, true); // smooth the glyph curves

	// Edge-pinned axis labels. Computed live from the current view each frame -
	// nothing is stored, so there's no stale-on-resize or zoom-drift to worry
	// about. x/x' ride the left+right edges at the x-axis's height; y/y' ride
	// the top+bottom edges at the y-axis's column. The origin's screen position
	// gives both axis lines.
	const double margin = 20.0;
	const QPointF origin = m_vp.worldToScreen(0, 0);

	// Clamp the axis coordinate so a label sticks to the corner instead of
	// vanishing when the axis itself is panned off-screen - that's what makes
	// the labels read as a compass rather than as marks on the plane.
	const double axisY = std::clamp(origin.y(), margin, height() - margin);
	const double axisX = std::clamp(origin.x(), margin, width() - margin);

	// Off-screen pointers: a label gains an arrow toward its sign-region once
	// that region has scrolled fully off the view (you panned along the axis).
	// The arrow always points the way the label denotes.
	const QString xLabel  = QStringLiteral("x")       + (m_vp.xMax() < 0 ? QStringLiteral("→") : QString());
	const QString xpLabel = QStringLiteral("\u2212x") + (m_vp.xMin() > 0 ? QStringLiteral("←") : QString());
	const QString yLabel  = QStringLiteral("y")       + (m_vp.yMax() < 0 ? QStringLiteral("↑") : QString());
	const QString ypLabel = QStringLiteral("\u2212y") + (m_vp.yMin() > 0 ? QStringLiteral("↓") : QString());

	const double xOp = m_xPresence;
	if (xOp > 0.01) {
		p.setOpacity(xOp);
		// Right-align the right-edge label so its arrow can't run off the edge;
		// the others extend inward and are safe left-aligned.
		const double xw = QFontMetricsF(m_symbolFont).horizontalAdvance(xLabel);
		p.drawText(QPointF(width() - margin - xw, axisY - 8), xLabel);   // right
		p.drawText(QPointF(margin, axisY - 8), xpLabel); 			    // left
	}

	const double yOp = m_yPresence;
	if (yOp > 0.01) { p.setOpacity(yOp);
	/* draw y and y′ */
	p.drawText(QPointF(axisX + 10, margin + 8), yLabel);                // top
	p.drawText(QPointF(axisX + 10, height() - margin), ypLabel);   // bottom
	}

	// O labels the actual origin, so it stays world-anchored and unclamped -
	// it should leave the screen when the origin does, not lie about where 0 is.
	p.setOpacity(1.0);   // O unaffected — it slides off with the origin, no fade
	// Sit O in the lower-left quadrant with a fixed gap from BOTH axis lines,
	// sized from the glyph's own metrics instead of magic offsets so it clears
	// the crossing cleanly at any font size. Right edge sits `oGap` left of the
	// vertical axis; top sits `oGap` below the horizontal axis.
	const QFontMetricsF ofm(m_symbolFont);
	const double oGap = 6.0;
	const double ow = ofm.horizontalAdvance(QStringLiteral("O"));
	p.drawText(QPointF(origin.x() - oGap - ow, origin.y() + oGap + ofm.ascent()), QStringLiteral("O"));
}

// Format a plain double
inline static QString fmtD(double v)
{
	if (std::isnan(v))
		return QStringLiteral("NaN");
	if (std::isinf(v))
		return v < 0 ? QStringLiteral("-Inf") : QStringLiteral("Inf");

	QString s = QString::number(v, 'g', 10);

	// normalize negative zero
	if (s == "-0")
		s = "0";

	return s;
}

void PlotCanvas::drawTickLabels(QPainter& p) {
	double xMin = m_vp.xMin();
	double xMax = m_vp.xMax();
	double yMin = m_vp.yMin();
	double yMax = m_vp.yMax();
	p.setFont(m_numberFont);
	p.setRenderHint(QPainter::TextAntialiasing, true);
	double step = m_vp.gridStep();
	QPointF origin = m_vp.worldToScreen(0, 0);
	const double margin = 20.0;

	const double axisY = std::clamp(origin.y(), margin, height() - margin);
	const double axisX = std::clamp(origin.x(), margin, width() - margin);

	double digits = std::max(0.0, -std::floor(std::log10(step) + 1e-9));
	QFontMetrics fm(m_numberFont);




	
	const double xOp = m_xPresence;
	if (xOp > 0.01) {
		p.setOpacity(xOp);
		for (double v = std::ceil(xMin / step) * step; v <= xMax; v += step) {
		if (std::abs(v) < step * 0.5) continue; //skip 0, O marks origin
		 QString s  = fmtD(v);
		 double sx = std::round(m_vp.worldToScreen(v, 0).x());

		 p.drawText(sx - fm.horizontalAdvance(NumberFormat::groupUnconditional(s)) / 2, axisY + 14 + fm.ascent(), NumberFormat::groupUnconditional(s));
		 QPen pen("#FFFFFF");
		 pen.setWidth(2);
		 p.save();
		 p.setPen(pen);
		 p.drawLine(sx, axisY - 8, sx, axisY + 8);
		 p.restore();
	}

}

	const double yOp = m_yPresence;
	if (yOp > 0.01) {
		p.setOpacity(yOp);
		for (double v = std::ceil(yMin / step) * step; v <= yMax; v += step) {
				if (std::abs(v) < step * 0.5) continue;
				QString s = fmtD(v);
				double sy = std::round(m_vp.worldToScreen(0, v).y());
				p.drawText(axisX - 20 - fm.horizontalAdvance(NumberFormat::groupUnconditional(s)), sy + fm.ascent() / 2, NumberFormat::groupUnconditional(s));
				QPen pen("#FFFFFF");
				pen.setWidth(2);
				p.save();
				p.setPen(pen);
				p.drawLine(axisX - 12, sy, axisX + 13, sy);
				p.restore();
	
				
			}

	}
	
}
