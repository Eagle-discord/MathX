#include "../RenderShape.h"

class Cylinder : public RenderShape {
public: 
	void draw() override;
	void setParameters(const QMap<QString, double>& params) override;
private:
	double m_radius = 0.0;
	double m_height = 0.0;
	double segments = 0.0;
};