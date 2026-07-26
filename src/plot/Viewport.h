#pragma once
#include <QPointF>

class Viewport {
public:
	void setViewportSize(int wPx, int hPx);
	void reset();

	QPointF worldToScreen(double wx, double wy) const;
	QPointF screenToWorld(double sx, double sy) const;

	void panByPixels(double dxPx, double dyPx);
	void zoomAt(double sxPx, double syPx, double factor);

	double xMin() const;
	double xMax() const;
	double yMin() const;
	double yMax() const;

	double scale() const { return m_scale; };
	double gridStep() const;
	double minorGridStep()  const;

	static void selfTest(); // Temporary

private:
	void decompose(double& mantissa, double& magnitude) const;
	static constexpr double kDefaultScale    = 40.0f;
	static constexpr double kMinScale        = 1e-6;
	static constexpr double kMaxScale		 = 1e6;
	static constexpr int	kTargetDivisions = 10;

	double m_centerX = 0.0;
	double m_centerY = 0.0;
	double m_scale   = kDefaultScale;
	int m_widthPx    = 0;
	int m_heightPx   = 0;

};