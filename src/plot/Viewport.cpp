#include "Viewport.h"
#include <algorithm>
#include <cmath>
#include <QDebug>

void Viewport::setViewportSize(int wPx, int hPx) {
	m_widthPx = wPx;
	m_heightPx = hPx;
}

void Viewport::reset()
{
	m_centerX = 0.0;
	m_centerY = 0.0;
    m_scale = kDefaultScale;
}

QPointF Viewport::worldToScreen(double wx, double wy) const
{
	double sx = m_widthPx * 0.5 + (wx - m_centerX) * scale(), sy = m_heightPx * 0.5 - (wy - m_centerY) * scale();
	return QPointF(sx, sy);
}

QPointF Viewport::screenToWorld(double sx, double sy) const
{
	double wx = m_centerX + (sx - m_widthPx * 0.5) / scale(), wy = m_centerY - (sy - m_heightPx * 0.5) / scale();
	return QPointF(wx, wy);
}

void Viewport::panByPixels(double dxPx, double dyPx)
{
	m_centerX -= dxPx / scale();
	m_centerY += dyPx / scale();
    
}

void Viewport::zoomAt(double sxPx, double syPx, double factor)
{
	QPointF before = screenToWorld(sxPx, syPx);
	m_scale = std::clamp(m_scale * factor, kMinScale, kMaxScale);
	QPointF after = screenToWorld(sxPx, syPx);
	m_centerX += before.x() - after.x();
	m_centerY += before.y() - after.y(); 
}

double Viewport::xMin() const
{
	return m_centerX - (m_widthPx * 0.5) / scale();
    qDebug() << m_centerX - (m_widthPx * 0.5) / scale();
}

double Viewport::xMax() const
{
	return m_centerX + (m_widthPx * 0.5) / scale();
    qDebug() << m_centerX + (m_widthPx * 0.5) / scale();
}

double Viewport::yMin() const
{
	return m_centerY - (m_heightPx * 0.5) / scale();
    qDebug() << m_centerY - (m_heightPx * 0.5) / scale();
}

double Viewport::yMax() const
{
	return m_centerY + (m_heightPx * 0.5) / scale();
    qDebug() << m_centerY + (m_heightPx * 0.5) / scale();
}

void Viewport::decompose(double& mantissa, double& magnitude) const {
    double visibleWidth = m_widthPx / scale();
    if (visibleWidth <= 0) {
        mantissa = 1;
        magnitude = 1;
        return;
    }

    double raw = visibleWidth / kTargetDivisions;
    magnitude = pow(10, floor(log10(raw)));
    double normalized = raw / magnitude;

    if (normalized <= 1 + 1e-9) mantissa = 1;
    else if (normalized <= 2 + 1e-9) mantissa = 2;
    else if (normalized <= 5 + 1e-9) mantissa = 5;
    else { mantissa = 1; magnitude *= 10; }
}

double Viewport::gridStep() const
{
    double magnitude, mantissa;
    decompose(mantissa, magnitude);
    return mantissa * magnitude;
}

double Viewport::minorGridStep() const
{
    double magnitude, mantissa, divisor;
    decompose(mantissa, magnitude);
    if (mantissa < 1.5) divisor = 5;
    else if (mantissa < 3) divisor = 4;
    else divisor = 5;
    return (magnitude * mantissa) / divisor;
}




void Viewport::selfTest() {
    int passed = 0, failed = 0;
    auto nearly = [](double a, double b) { return std::fabs(a - b) < 1e-9; };
    auto check = [&](const char* name, bool ok) {
        if (ok) ++passed;
        else { ++failed; qWarning("Viewport selfTest FAILED: %s", name); }
        };

    {   // transforms
        Viewport v; v.setViewportSize(800, 600); v.reset();
        QPointF c = v.worldToScreen(0, 0);
        check("origin -> widget centre", nearly(c.x(), 400) && nearly(c.y(), 300));

        QPointF rx = v.worldToScreen(1, 0);
        check("+1 world x -> +40 px", nearly(rx.x(), 440) && nearly(rx.y(), 300));

        QPointF uy = v.worldToScreen(0, 1);
        check("+1 world y -> UP screen", nearly(uy.x(), 400) && nearly(uy.y(), 260));

        QPointF w = v.screenToWorld(400, 300);
        check("centre px -> origin", nearly(w.x(), 0) && nearly(w.y(), 0));

        QPointF s = v.worldToScreen(3, -2);
        QPointF rt = v.screenToWorld(s.x(), s.y());
        check("round trip", nearly(rt.x(), 3) && nearly(rt.y(), -2));
    }

    {   // bounds
        Viewport v; v.setViewportSize(800, 600); v.reset();
        check("xMin/xMax", nearly(v.xMin(), -10.0) && nearly(v.xMax(), 10.0));
        check("yMin/yMax", nearly(v.yMin(), -7.5) && nearly(v.yMax(), 7.5));
    }

    {   // pan
        Viewport v; v.setViewportSize(800, 600); v.reset();
        v.panByPixels(40, 0);
        QPointF c = v.worldToScreen(0, 0);
        check("pan right moves content right", nearly(c.x(), 440) && nearly(c.y(), 300));

        v.panByPixels(-40, 0);
        QPointF b = v.worldToScreen(0, 0);
        check("pan and return", nearly(b.x(), 400) && nearly(b.y(), 300));
    }

    {   // zoom anchored at the TOP-LEFT CORNER, not the centre
        Viewport v; v.setViewportSize(800, 600); v.reset();
        QPointF before = v.screenToWorld(0, 0);
        check("corner before zoom", nearly(before.x(), -10.0) && nearly(before.y(), 7.5));

        v.zoomAt(0, 0, 2.0);
        QPointF after = v.screenToWorld(0, 0);
        check("zoom anchor invariant",
            nearly(after.x(), before.x()) && nearly(after.y(), before.y()));
        check("scale doubled", nearly(v.scale(), 80.0));
    }

    {   // grid step
        Viewport v; v.setViewportSize(800, 600); v.reset();
        check("gridStep at scale 40", nearly(v.gridStep(), 2.0));
        v.zoomAt(400, 300, 4.0);                    // scale 40 -> 160
        check("gridStep at scale 160", nearly(v.gridStep(), 0.5));
    }

    {   // minor grid step - shares decompose() with gridStep
        Viewport v; v.setViewportSize(800, 600); v.reset();
        // scale 40: major 2 (mantissa 2) -> divisor 4 -> minor 0.5
        check("minorGridStep at scale 40", nearly(v.minorGridStep(), 0.5));
        v.zoomAt(400, 300, 4.0);                    // scale 40 -> 160
        // scale 160: major 0.5 (mantissa 5) -> divisor 5 -> minor 0.1
        check("minorGridStep at scale 160", nearly(v.minorGridStep(), 0.1));
    }

    qDebug("Viewport selfTest: %d passed, %d failed", passed, failed);
}

