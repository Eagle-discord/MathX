#include "shapes/Shapes2D.h"
#include "math/BigNum.h"
#include "constants/Theme.h"
#include <QTimer>
#include <cmath>
#include <algorithm>

#define BD BigNum::bd
#define PI BigNum::PI
#define PHI BigNum::PHI
#define FMT BigNum::fmt
#define SQ  BigNum::sqrt
#define AC  BigNum::acos
#define AS  BigNum::asin
#define SIN BigNum::sin
#define COS BigNum::cos
#define TAN BigNum::tan
#define POW BigNum::pow
#define DEFER QTimer::singleShot(0,this,[this]{recompute();});

// -- Circle --------------------------------------------------------------------
CircleCard::CircleCard(double r, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Circle", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    addSlider("r", r, 0.1, geoAutoMax(r), geoAutoStep(r));
    m_layout->addWidget(geoDiv());
    addResult("Diameter      :", "2r");
    addResult("Circumference :", "2\xcf\x80r");
    addResult("Area          :", "\xcf\x80r\xc2\xb2");
    DEFER;
    m_shapeType = "circle";

}
void CircleCard::recompute() {
    double r = m_sliders["r"]->value();   // ensure this returns double
    BigDec bdr(r);
    m_rows["Diameter      :"]->setValue(BigNum::fmt(2 * bdr));
    m_rows["Circumference :"]->setValue(BigNum::fmt(2 * PI * bdr));
    m_rows["Area          :"]->setValue(BigNum::fmt(PI * bdr * bdr));
}
// -- Ellipse -------------------------------------------------------------------
EllipseCard::EllipseCard(double a, double b, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Ellipse", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    double mx = std::max(a, b) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("a", a, 0.1, mx, step);
    addSlider("b", b, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Area            :", "\xcf\x80 ab");
    addResult("Perimeter \xe2\x89\x88     :", "Ramanujan approx");
    addResult("Eccentricity    :", "\xe2\x88\x9a(1-b\xc2\xb2/a\xc2\xb2)");
    addResult("Focal Distance  :", "\xe2\x88\x9a(a\xc2\xb2-b\xc2\xb2)");
    DEFER
}
void EllipseCard::recompute() {
    BigDec a(m_sliders["a"]->value()), b(m_sliders["b"]->value());
    BigDec h = (a - b) * (a - b) / ((a + b) * (a + b));
    BigDec perim = PI * (a + b) * (1 + BD(3) * h / (10 + SQ(4 - BD(3) * h)));
    BigDec bigA = (a >= b) ? a : b, bigB = (a >= b) ? b : a;
    BigDec ecc = SQ(1 - (bigB * bigB) / (bigA * bigA));
    BigDec focal = SQ(bigA * bigA - bigB * bigB);
    m_rows["Area            :"]->setValue(FMT(PI * a * b));
    m_rows["Perimeter \xe2\x89\x88     :"]->setValue(FMT(perim));
    m_rows["Eccentricity    :"]->setValue(FMT(ecc));
    m_rows["Focal Distance  :"]->setValue(FMT(focal));
}

// -- Triangle (SSS) ------------------------------------------------------------
TriangleCard::TriangleCard(double a, double b, double c, QWidget* p) : GeoCard(p) {

    m_layout->addWidget(geoTitle("Triangle (SSS)", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    double mx = std::max({ a,b,c }) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("a", a, 0.1, mx, step);
    addSlider("b", b, 0.1, mx, step);
    addSlider("c", c, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Perimeter   :", "a + b + c");
    addResult("Semi-perim  :", "s = (a+b+c)/2");
    addResult("Area        :", "\xe2\x88\x9a(s(s-a)(s-b)(s-c))");
    addResult("Angle A     :", "arccos((b\xc2\xb2+c\xc2\xb2-a\xc2\xb2)/(2bc))");
    addResult("Angle B     :", "arccos((a\xc2\xb2+c\xc2\xb2-b\xc2\xb2)/(2ac))");
    addResult("Angle C     :", "180\xc2\xb0 - A - B");
    addResult("Inradius    :", "Area / s");
    addResult("Circumradius:", "abc / (4\xc2\xb7 Area)");
    DEFER
}
void TriangleCard::recompute() {

    BigDec a(m_sliders["a"]->value()), b(m_sliders["b"]->value()), c(m_sliders["c"]->value());
    if (a + b <= c || a + c <= b || b + c <= a) {
        for (auto& k : m_rows.keys()) m_rows[k]->setValue("\xe2\x9a\xa0 invalid triangle");
        return;
    }
    BigDec s = (a + b + c) / 2, area = SQ(s * (s - a) * (s - b) * (s - c));
    BigDec A = AC((b * b + c * c - a * a) / (2 * b * c)) * 180 / PI;
    BigDec B = AC((a * a + c * c - b * b) / (2 * a * c)) * 180 / PI;
    m_rows["Perimeter   :"]->setValue(FMT(a + b + c));
    m_rows["Semi-perim  :"]->setValue(FMT(s));
    m_rows["Area        :"]->setValue(FMT(area));
    m_rows["Angle A     :"]->setValue(FMT(A) + "\xc2\xb0");
    m_rows["Angle B     :"]->setValue(FMT(B) + "\xc2\xb0");
    m_rows["Angle C     :"]->setValue(FMT(180 - A - B) + "\xc2\xb0");
    m_rows["Inradius    :"]->setValue(FMT(area / s));
    m_rows["Circumradius:"]->setValue(FMT(a * b * c / (4 * area)));
}

// -- Right Triangle ------------------------------------------------------------
RightTriCard::RightTriCard(double a, double b, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Right Triangle", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    double mx = std::max(a, b) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("a", a, 0.1, mx, step);
    addSlider("b", b, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Hypotenuse  :", "\xe2\x88\x9a(a\xc2\xb2+b\xc2\xb2)");
    addResult("Perimeter   :", "a + b + c");
    addResult("Area        :", "(a \xc3\x97 b) / 2");
    addResult("Angle A     :", "arctan(a/b)");
    addResult("Angle B     :", "arctan(b/a)");
    addResult("Inradius    :", "(a+b-c)/2");
    addResult("Circumradius:", "c/2");
    DEFER
}
void RightTriCard::recompute() {
    BigDec a(m_sliders["a"]->value()), b(m_sliders["b"]->value());
    BigDec c = SQ(a * a + b * b);
    BigDec A = AC(b / c) * 180 / PI;
    BigDec B = AC(a / c) * 180 / PI;
    m_rows["Hypotenuse  :"]->setValue(FMT(c));
    m_rows["Perimeter   :"]->setValue(FMT(a + b + c));
    m_rows["Area        :"]->setValue(FMT(a * b / 2));
    m_rows["Angle A     :"]->setValue(FMT(A) + "\xc2\xb0");
    m_rows["Angle B     :"]->setValue(FMT(B) + "\xc2\xb0");
    m_rows["Inradius    :"]->setValue(FMT((a + b - c) / 2));
    m_rows["Circumradius:"]->setValue(FMT(c / 2));
}

// -- Rectangle -----------------------------------------------------------------
RectCard::RectCard(double w, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Rectangle", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    double mx = std::max(w, h) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("w", w, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Perimeter :", "2(w+h)");
    addResult("Area      :", "w \xc3\x97 h");
    addResult("Diagonal  :", "\xe2\x88\x9a(w\xc2\xb2+h\xc2\xb2)");
    DEFER
}
void RectCard::recompute() {
    BigDec w(m_sliders["w"]->value()), h(m_sliders["h"]->value());
    m_rows["Perimeter :"]->setValue(FMT(2 * (w + h)));
    m_rows["Area      :"]->setValue(FMT(w * h));
    m_rows["Diagonal  :"]->setValue(FMT(SQ(w * w + h * h)));
}

// -- Square --------------------------------------------------------------------
SquareCard::SquareCard(double s, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Square", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    addSlider("s", s, 0.1, geoAutoMax(s), geoAutoStep(s));
    m_layout->addWidget(geoDiv());
    addResult("Perimeter :", "4s");
    addResult("Area      :", "s\xc2\xb2");
    addResult("Diagonal  :", "s\xe2\x88\x9a\x32");
    DEFER
}
void SquareCard::recompute() {
    BigDec s(m_sliders["s"]->value());
    m_rows["Perimeter :"]->setValue(FMT(4 * s));
    m_rows["Area      :"]->setValue(FMT(s * s));
    m_rows["Diagonal  :"]->setValue(FMT(s * SQ(BD(2))));
}

// -- Parallelogram -------------------------------------------------------------
ParallelCard::ParallelCard(double b, double h, double s, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Parallelogram", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    double mx = std::max({ b,h,s }) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("b", b, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    addSlider("s", s, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Perimeter :", "2(b+s)");
    addResult("Area      :", "b \xc3\x97 h");
    DEFER
}
void ParallelCard::recompute() {
    BigDec b(m_sliders["b"]->value()), h(m_sliders["h"]->value()), s(m_sliders["s"]->value());
    m_rows["Perimeter :"]->setValue(FMT(2 * (b + s)));
    m_rows["Area      :"]->setValue(FMT(b * h));
}

// -- Trapezoid -----------------------------------------------------------------
TrapezoidCard::TrapezoidCard(double a, double b, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Trapezoid", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    double mx = std::max({ a,b,h }) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("a", a, 0.1, mx, step);
    addSlider("b", b, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Area     :", "(a+b)/2 \xc3\x97 h");
    addResult("Mid-line :", "(a+b)/2");
    DEFER
}
void TrapezoidCard::recompute() {
    BigDec a(m_sliders["a"]->value()), b(m_sliders["b"]->value()), h(m_sliders["h"]->value());
    m_rows["Area     :"]->setValue(FMT((a + b) / 2 * h));
    m_rows["Mid-line :"]->setValue(FMT((a + b) / 2));
}

// -- Rhombus -------------------------------------------------------------------
RhombusCard::RhombusCard(double d1, double d2, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Rhombus", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    double mx = std::max(d1, d2) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("d1", d1, 0.1, mx, step);
    addSlider("d2", d2, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Side      :", "\xe2\x88\x9a((d1\xc2\xb2+d2\xc2\xb2)/4)");
    addResult("Perimeter :", "4 \xc3\x97 side");
    addResult("Area      :", "(d1 \xc3\x97 d2)/2");
    DEFER
}
void RhombusCard::recompute() {
    BigDec d1(m_sliders["d1"]->value()), d2(m_sliders["d2"]->value());
    BigDec side = SQ((d1 * d1 + d2 * d2) / 4);
    m_rows["Side      :"]->setValue(FMT(side));
    m_rows["Perimeter :"]->setValue(FMT(4 * side));
    m_rows["Area      :"]->setValue(FMT(d1 * d2 / 2));
}

// -- Regular Polygon -----------------------------------------------------------
RegPolyCard::RegPolyCard(int n, double s, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle(QString("Regular %1-gon").arg(n)));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    addSlider("s", s, 0.1, geoAutoMax(s), geoAutoStep(s));
    m_layout->addWidget(geoDiv());
    addResult("Perimeter   :", "n \xc3\x97 s");
    addResult("Area        :", "(ns\xc2\xb2)/(4tan(\xcf\x80/n))");
    addResult("Interior \xe2\x88\xa0  :", "(n-2)\xc3\x97 180\xc2\xb0/n");
    addResult("Apothem     :", "s/(2tan(\xcf\x80/n))");
    addResult("Circumradius:", "s/(2sin(\xcf\x80/n))");
    setProperty("poly_n", n);
    DEFER
}
void RegPolyCard::recompute() {
    int n = property("poly_n").toInt();
    BigDec s(m_sliders["s"]->value());
    BigDec piN = PI / BD(n);
    BigDec tanPN = TAN(piN), sinPN = SIN(piN);
    m_rows["Perimeter   :"]->setValue(FMT(BD(n) * s));
    m_rows["Area        :"]->setValue(FMT(BD(n) * s * s / (4 * tanPN)));
    m_rows["Interior \xe2\x88\xa0  :"]->setValue(FMT(BD((n - 2) * 180) / BD(n)) + "\xc2\xb0");
    m_rows["Apothem     :"]->setValue(FMT(s / (2 * tanPN)));
    m_rows["Circumradius:"]->setValue(FMT(s / (2 * sinPN)));
}

// -- Sector --------------------------------------------------------------------
SectorCard::SectorCard(double r, double deg, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Circle Sector", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    addSlider("r", r, 0.1, geoAutoMax(r), geoAutoStep(r));
    addSlider("deg", deg, 1.0, 360.0, 1.0);
    m_layout->addWidget(geoDiv());
    addResult("Arc Length  :", "r\xce\xb8  (\xce\xb8 in radians)");
    addResult("Area        :", "r\xc2\xb2\xce\xb8/2");
    addResult("Chord       :", "2r\xc2\xb7sin(\xce\xb8/2)");
    addResult("Perimeter   :", "arc + 2r");
    DEFER
}
void SectorCard::recompute() {
    BigDec r(m_sliders["r"]->value());
    BigDec theta = BD(m_sliders["deg"]->value()) * PI / 180;
    BigDec arc = r * theta;
    m_rows["Arc Length  :"]->setValue(FMT(arc));
    m_rows["Area        :"]->setValue(FMT(r * r * theta / 2));
    m_rows["Chord       :"]->setValue(FMT(2 * r * SIN(theta / 2)));
    m_rows["Perimeter   :"]->setValue(FMT(arc + 2 * r));
}

// -- Annulus -------------------------------------------------------------------
AnnulusCard::AnnulusCard(double R, double r, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Annulus (Ring)", this));
    m_layout->addWidget(geoHint());
    m_layout->addWidget(geoDiv());
    double mx = std::max(R, r) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("R", R, 0.1, mx, step);
    addSlider("r", r, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Area         :", "\xcf\x80(R\xc2\xb2-r\xc2\xb2)");
    addResult("Outer Circum :", "2\xcf\x80R");
    addResult("Inner Circum :", "2\xcf\x80r");
    addResult("Width        :", "R - r");
    DEFER
}
void AnnulusCard::recompute() {
    BigDec R(m_sliders["R"]->value()), r(m_sliders["r"]->value());
    if (R <= r) { m_rows["Area         :"]->setValue("\xe2\x9a\xa0 R must be > r"); return; }
    m_rows["Area         :"]->setValue(FMT(PI * (R * R - r * r)));
    m_rows["Outer Circum :"]->setValue(FMT(2 * PI * R));
    m_rows["Inner Circum :"]->setValue(FMT(2 * PI * r));
    m_rows["Width        :"]->setValue(FMT(R - r));
}