#include "Shapes3D.h"
#include "../math/BigNum.h"
#include "../constants/Theme.h"
#include <QTimer>
#include <cmath>
#include <algorithm>
#include "../render/RenderWidget.h"
#include <iostream>

#define DEFER QTimer::singleShot(0,this,[this]{recompute();});recompute();
using BigNum::PHI;
using BigNum::PI;
//  3D SHAPES
// ------------------------------------------------------------------------------

// -- Sphere --------------------------------------------------------------------
SphereCard::SphereCard(double r, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Sphere", this));
    addSlider("r", r, 0.1, geoAutoMax(r), geoAutoStep(r));
    m_layout->addWidget(geoDiv());
    addResult("Diameter     :", "2r");
    addResult("Surface Area :", "4πr²");
    addResult("Volume       :", "(4/3)πr³");
    addResult("Great Circle :", "2πr");
    DEFER;
    m_shapeType = "sphere";
}

void SphereCard::recompute() {
    double r = m_sliders["r"]->value();
    // update text rows
    m_rows["Diameter     :"]->setValue(QString::number(2 * r, 'g', 10));
    m_rows["Surface Area :"]->setValue(QString::number(4 * M_PI * r * r, 'g', 10));
    m_rows["Volume       :"]->setValue(QString::number(4.0 / 3.0 * M_PI * r * r * r, 'g', 10));
    m_rows["Great Circle :"]->setValue(QString::number(2 * M_PI * r, 'g', 10));

    
}

// -- Hemisphere ----------------------------------------------------------------
HemiSphereCard::HemiSphereCard(double r, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Hemisphere", this));
    addSlider("r", r, 0.1, geoAutoMax(r), geoAutoStep(r));
    m_layout->addWidget(geoDiv());
    addResult("Curved SA   :", "2\xcf\x80r\xc2\xb2");
    addResult("Base Area   :", "  \xcf\x80r\xc2\xb2");
    addResult("Total SA    :", "3\xcf\x80r\xc2\xb2");
    addResult("Volume      :", "(2/3)\xcf\x80r\xc2\xb3");
    DEFER
}
void HemiSphereCard::recompute() {
    BigDec r(m_sliders["r"]->value());
    m_rows["Curved SA   :"]->setValue(BigNum::fmt(2 * BigNum::PI * r * r));
    m_rows["Base Area   :"]->setValue(BigNum::fmt(BigNum::PI * r * r));
    m_rows["Total SA    :"]->setValue(BigNum::fmt(3 * BigNum::PI * r * r));
    m_rows["Volume      :"]->setValue(BigNum::fmt(BigNum::bd(2) / 3 * BigNum::PI * r * r * r));
}

// -- Cylinder ------------------------------------------------------------------
CylinderCard::CylinderCard(double r, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Cylinder", this));
    double mx = std::max(r, h) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("r", r, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Lateral Area :", "2\xcf\x80rh");
    addResult("Base Area    :", "  \xcf\x80r\xc2\xb2");
    addResult("Total Area   :", "2\xcf\x80r(r+h)");
    addResult("Volume       :", "  \xcf\x80r\xc2\xb2h");
    DEFER;
    m_shapeType = "cylinder";
}
void CylinderCard::recompute() {
    BigDec r(m_sliders["r"]->value()), h(m_sliders["h"]->value());
    m_rows["Lateral Area :"]->setValue(BigNum::fmt(2 * BigNum::PI * r * h));
    m_rows["Base Area    :"]->setValue(BigNum::fmt(BigNum::PI * r * r));
    m_rows["Total Area   :"]->setValue(BigNum::fmt(2 * BigNum::PI * r * (r + h)));
    m_rows["Volume       :"]->setValue(BigNum::fmt(BigNum::PI * r * r * h));
}

// -- Hollow Cylinder -----------------------------------------------------------
HollowCylCard::HollowCylCard(double R, double r, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Hollow Cylinder", this));
    double mx = std::max({ R,r,h }) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("R (outer)", R, 0.1, mx, step);
    addSlider("r (inner)", r, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Volume       :", "  \xcf\x80h(R\xc2\xb2-r\xc2\xb2)");
    addResult("Lateral Area :", "2\xcf\x80h(R+r)");
    addResult("Total Area   :", "Lateral + 2\xcf\x80(R\xc2\xb2-r\xc2\xb2)");
    addResult("Wall Thick   :", "R - r");
    DEFER
}
void HollowCylCard::recompute() {
    BigDec R(m_sliders["R (outer)"]->value()), r(m_sliders["r (inner)"]->value()), h(m_sliders["h"]->value());
    if (R <= r) { m_rows["Volume       :"]->setValue("\xe2\x9a\xa0 R must be > r"); return; }
    BigDec lat = 2 * BigNum::PI * h * (R + r);
    BigDec ring = 2 * BigNum::PI * (R * R - r * r);
    m_rows["Volume       :"]->setValue(BigNum::fmt(BigNum::PI * h * (R * R - r * r)));
    m_rows["Lateral Area :"]->setValue(BigNum::fmt(lat));
    m_rows["Total Area   :"]->setValue(BigNum::fmt(lat + ring));
    m_rows["Wall Thick   :"]->setValue(BigNum::fmt(R - r));
}

// -- Cone ----------------------------------------------------------------------
ConeCard::ConeCard(double r, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Cone", this));
    double mx = std::max(r, h) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("r", r, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Slant Height :", "l = \xe2\x88\x9a(r\xc2\xb2+h\xc2\xb2)");
    addResult("Base Area    :", "  \xcf\x80r\xc2\xb2");
    addResult("Lateral Area :", "  \xcf\x80rl");
    addResult("Total Area   :", "  \xcf\x80r(r+l)");
    addResult("Volume       :", "(1/3)\xcf\x80r\xc2\xb2h");
    DEFER
}
void ConeCard::recompute() {
    BigDec r(m_sliders["r"]->value()), h(m_sliders["h"]->value());
    BigDec l = BigNum::sqrt(r * r + h * h);
    m_rows["Slant Height :"]->setValue(BigNum::fmt(l));
    m_rows["Base Area    :"]->setValue(BigNum::fmt(BigNum::PI * r * r));
    m_rows["Lateral Area :"]->setValue(BigNum::fmt(BigNum::PI * r * l));
    m_rows["Total Area   :"]->setValue(BigNum::fmt(BigNum::PI * r * (r + l)));
    m_rows["Volume       :"]->setValue(BigNum::fmt(BigNum::PI * r * r * h / 3));
}

// -- Frustum (Truncated Cone) --------------------------------------------------
FrustumCard::FrustumCard(double R, double r, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Frustum (Truncated Cone)", this));
    double mx = std::max({ R,r,h }) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("R (base)", R, 0.1, mx, step);
    addSlider("r (top)", r, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Slant Height :", "l = \xe2\x88\x9a(h\xc2\xb2+(R-r)\xc2\xb2)");
    addResult("Lateral Area :", "  \xcf\x80(R+r)l");
    addResult("Total Area   :", "  \xcf\x80(R\xc2\xb2+r\xc2\xb2+(R+r)l)");
    addResult("Volume       :", "(\xcf\x80h/3)(R\xc2\xb2+Rr+r\xc2\xb2)");
    DEFER
}
void FrustumCard::recompute() {
    BigDec R(m_sliders["R (base)"]->value()), r(m_sliders["r (top)"]->value()), h(m_sliders["h"]->value());
    BigDec l = BigNum::sqrt(h * h + (R - r) * (R - r));
    m_rows["Slant Height :"]->setValue(BigNum::fmt(l));
    m_rows["Lateral Area :"]->setValue(BigNum::fmt(BigNum::PI * (R + r) * l));
    m_rows["Total Area   :"]->setValue(BigNum::fmt(BigNum::PI * (R * R + r * r + (R + r) * l)));
    m_rows["Volume       :"]->setValue(BigNum::fmt(BigNum::PI * h / 3 * (R * R + R * r + r * r)));
}

// -- Cuboid --------------------------------------------------------------------
CuboidCard::CuboidCard(double l, double w, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Cuboid", this));
    double mx = std::max({ l,w,h }) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("l", l, 0.1, mx, step);
    addSlider("w", w, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Surface Area :", "2(lw+lh+wh)");
    addResult("Volume       :", "l \xc3\x97 w \xc3\x97 h");
    addResult("Diagonal     :", "d = \xe2\x88\x9a(l\xc2\xb2+w\xc2\xb2+h\xc2\xb2)");
    addResult("Face Diag lw :", "d = \xe2\x88\x9a(l\xc2\xb2+w\xc2\xb2)");
    DEFER
}
void CuboidCard::recompute() {
    BigDec l(m_sliders["l"]->value()), w(m_sliders["w"]->value()), h(m_sliders["h"]->value());
    m_rows["Surface Area :"]->setValue(BigNum::fmt(2 * (l * w + l * h + w * h)));
    m_rows["Volume       :"]->setValue(BigNum::fmt(l * w * h));
    m_rows["Diagonal     :"]->setValue(BigNum::fmt(BigNum::sqrt(l * l + w * w + h * h)));
    m_rows["Face Diag lw :"]->setValue(BigNum::fmt(BigNum::sqrt(l * l + w * w)));
}

// -- Cube ----------------------------------------------------------------------
CubeCard::CubeCard(double s, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Cube", this));
    addSlider("s", s, 0.1, geoAutoMax(s), geoAutoStep(s));
    m_layout->addWidget(geoDiv());
    addResult("Surface Area :", "6s\xc2\xb2");
    addResult("Volume       :", "s\xc2\xb3");
    addResult("Face Diagonal:", "s\xe2\x88\x9a\x32");
    addResult("Space Diagonal:", "s\xe2\x88\x9a\x33");
    addResult("Inradius     :", "s/2");
    addResult("Circumradius :", "s\xe2\x88\x9a\x33/2");
    DEFER;
    m_shapeType = "cube";
}
void CubeCard::recompute() {
    BigDec s(m_sliders["s"]->value());
    double s_doub(m_sliders["s"]->value());
    m_rows["Surface Area :"]->setValue(BigNum::fmt(6 * s * s));
    m_rows["Volume       :"]->setValue(BigNum::fmt(s * s * s));
    m_rows["Face Diagonal:"]->setValue(BigNum::fmt(s * BigNum::sqrt(BigNum::bd(2))));
    m_rows["Space Diagonal:"]->setValue(BigNum::fmt(s * BigNum::sqrt(BigNum::bd(3))));
    m_rows["Inradius     :"]->setValue(BigNum::fmt(s / 2));
    m_rows["Circumradius :"]->setValue(BigNum::fmt(s * BigNum::sqrt(BigNum::bd(3)) / 2));


}

// -- Tetrahedron ---------------------------------------------------------------
TetraCard::TetraCard(double s, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Regular Tetrahedron", this));
    addSlider("s", s, 0.1, geoAutoMax(s), geoAutoStep(s));
    m_layout->addWidget(geoDiv());
    addResult("Surface Area :", "s\xc2\xb2\xe2\x88\x9a\x33");
    addResult("Volume       :", "s\xc2\xb3/(6\xe2\x88\x9a\x32)");
    addResult("Height       :", "s\xe2\x88\x9a(2/3)");
    addResult("Inradius     :", "s/(2\xe2\x88\x9a\x36)");
    addResult("Circumradius :", "s\xe2\x88\x9a\x36/4");
    DEFER
}
void TetraCard::recompute() {
    BigDec s(m_sliders["s"]->value());
    m_rows["Surface Area :"]->setValue(BigNum::fmt(s * s * BigNum::sqrt(BigNum::bd(3))));
    m_rows["Volume       :"]->setValue(BigNum::fmt(s * s * s / (6 * BigNum::sqrt(BigNum::bd(2)))));
    m_rows["Height       :"]->setValue(BigNum::fmt(s * BigNum::sqrt(BigNum::bd(2) / 3)));
    m_rows["Inradius     :"]->setValue(BigNum::fmt(s / (2 * BigNum::sqrt(BigNum::bd(6)))));
    m_rows["Circumradius :"]->setValue(BigNum::fmt(s * BigNum::sqrt(BigNum::bd(6)) / 4));
}

// -- Octahedron ----------------------------------------------------------------
OctaCard::OctaCard(double s, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Regular Octahedron", this));
    addSlider("s", s, 0.1, geoAutoMax(s), geoAutoStep(s));
    m_layout->addWidget(geoDiv());
    addResult("Surface Area :", "2s\xc2\xb2\xe2\x88\x9a\x33");
    addResult("Volume       :", "(s\xc2\xb3\xe2\x88\x9a\x32)/3");
    addResult("Inradius     :", "s\xe2\x88\x9a\x36/6");
    addResult("Circumradius :", "s\xe2\x88\x9a\x32/2");
    DEFER
}
void OctaCard::recompute() {
    BigDec s(m_sliders["s"]->value());
    m_rows["Surface Area :"]->setValue(BigNum::fmt(2 * s * s * BigNum::sqrt(BigNum::bd(3))));
    m_rows["Volume       :"]->setValue(BigNum::fmt(s * s * s * BigNum::sqrt(BigNum::bd(2)) / 3));
    m_rows["Inradius     :"]->setValue(BigNum::fmt(s * BigNum::sqrt(BigNum::bd(6)) / 6));
    m_rows["Circumradius :"]->setValue(BigNum::fmt(s * BigNum::sqrt(BigNum::bd(2)) / 2));
}

// -- Icosahedron ---------------------------------------------------------------
IcosaCard::IcosaCard(double s, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Regular Icosahedron", this));
    addSlider("s", s, 0.1, geoAutoMax(s), geoAutoStep(s));
    m_layout->addWidget(geoDiv());
    addResult("Surface Area :", "5s\xc2\xb2\xe2\x88\x9a\x33");
    addResult("Volume       :", "(5/12)s\xc2\xb3(3+\xe2\x88\x9a\x35)");
    addResult("Inradius     :", "s\xc3\xb8/4 (\xc3\xb8=golden ratio)");
    addResult("Circumradius :", "s\xe2\x88\x9a(10+2\xe2\x88\x9a\x35)/4");
    DEFER
}
void IcosaCard::recompute() {
    BigDec s(m_sliders["s"]->value());
    BigDec sqrt5 = BigNum::sqrt(BigNum::bd(5));
    m_rows["Surface Area :"]->setValue(BigNum::fmt(5 * s * s * BigNum::sqrt(BigNum::bd(3))));
    m_rows["Volume       :"]->setValue(BigNum::fmt(BigNum::bd(5) / 12 * s * s * s * (3 + sqrt5)));
    m_rows["Inradius     :"]->setValue(BigNum::fmt(s * PHI * PHI / sqrt(BigNum::bd(3) * PHI * PHI + 1)));
    m_rows["Circumradius :"]->setValue(BigNum::fmt(s * BigNum::sqrt(10 + 2 * sqrt5) / 4));
}

// -- Dodecahedron --------------------------------------------------------------
DodecaCard::DodecaCard(double s, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Regular Dodecahedron", this));
    addSlider("s", s, 0.1, geoAutoMax(s), geoAutoStep(s));
    m_layout->addWidget(geoDiv());
    addResult("Surface Area :", "3s\xc2\xb2\xe2\x88\x9a(25+10\xe2\x88\x9a\x35)");
    addResult("Volume       :", "(1/4)(15+7\xe2\x88\x9a\x35)s\xc2\xb3");
    addResult("Inradius     :", "s\xe2\x88\x9a(25+10\xe2\x88\x9a\x35)/\xe2\x88\x9a(20)");
    addResult("Circumradius :", "s\xe2\x88\x9a\x33\xc2\xb7\xcf\x86");
    DEFER
}
void DodecaCard::recompute() {
    BigDec s(m_sliders["s"]->value());
    BigDec sqrt5 = BigNum::sqrt(BigNum::bd(5));
    m_rows["Surface Area :"]->setValue(BigNum::fmt(3 * s * s * BigNum::sqrt(25 + 10 * sqrt5)));
    m_rows["Volume       :"]->setValue(BigNum::fmt(BigNum::bd(1) / 4 * (15 + 7 * sqrt5) * s * s * s));
    m_rows["Inradius     :"]->setValue(BigNum::fmt(s * BigNum::sqrt(25 + 10 * sqrt5) / sqrt(BigNum::bd(20))));
    m_rows["Circumradius :"]->setValue(BigNum::fmt(s * BigNum::sqrt(BigNum::bd(3)) * PHI));
}

// -- Prism (regular n-gon base) ------------------------------------------------
PrismCard::PrismCard(int n, double s, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle(QString("Regular %1-gon Prism").arg(n)));
    double mx = std::max(s, h) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("s (side)", s, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Base Area    :", "(ns\xc2\xb2)/(4tan(\xcf\x80/n))");
    addResult("Lateral Area :", "n\xc2\xb7s\xc2\xb7h");
    addResult("Total Area   :", "2\xc2\xb7 base + lateral");
    addResult("Volume       :", "base \xc3\x97 h");
    setProperty("prism_n", n);
    DEFER
}
void PrismCard::recompute() {
    int n = property("prism_n").toInt();
    BigDec s(m_sliders["s (side)"]->value()), h(m_sliders["h"]->value());
    BigDec piOverN = BigNum::PI / BigNum::bd(n);
    BigDec baseArea = BigNum::bd(n) * s * s / (4 * boost::multiprecision::tan(piOverN));
    BigDec lat = BigNum::bd(n) * s * h;
    m_rows["Base Area    :"]->setValue(BigNum::fmt(baseArea));
    m_rows["Lateral Area :"]->setValue(BigNum::fmt(lat));
    m_rows["Total Area   :"]->setValue(BigNum::fmt(2 * baseArea + lat));
    m_rows["Volume       :"]->setValue(BigNum::fmt(baseArea * h));
}

// -- Square Pyramid ------------------------------------------------------------
PyramidCard::PyramidCard(double b, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Square Pyramid", this));
    double mx = std::max(b, h) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("b (base)", b, 0.1, mx, step);
    addSlider("h", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Slant Height :", "l = \xe2\x88\x9a(h\xc2\xb2+(b/2)\xc2\xb2)");
    addResult("Base Area    :", "b\xc2\xb2");
    addResult("Lateral Area :", "2bl");
    addResult("Total Area   :", "b\xc2\xb2+2bl");
    addResult("Volume       :", "(1/3)b\xc2\xb2h");
    DEFER
}
void PyramidCard::recompute() {
    BigDec b(m_sliders["b (base)"]->value()), h(m_sliders["h"]->value());
    BigDec l = BigNum::sqrt(h * h + (b / 2) * (b / 2));
    m_rows["Slant Height :"]->setValue(BigNum::fmt(l));
    m_rows["Base Area    :"]->setValue(BigNum::fmt(b * b));
    m_rows["Lateral Area :"]->setValue(BigNum::fmt(2 * b * l));
    m_rows["Total Area   :"]->setValue(BigNum::fmt(b * b + 2 * b * l));
    m_rows["Volume       :"]->setValue(BigNum::fmt(b * b * h / 3));
}

// -- Torus ---------------------------------------------------------------------
TorusCard::TorusCard(double major, double minor, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Torus", this));
    // Set larger range for sliders, but limit minor < major dynamically
    double mx = std::max(major, minor) * 5;
    double step = mx > 50 ? 1.0 : 0.1;
    addSlider("major", major, 0.1, mx, step);
    addSlider("minor", minor, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Surface Area :", "4π² R r");
    addResult("Volume       :", "2π² R r²");
    // Store initial values to validate
    setProperty("initR", major);
    setProperty("initr", minor);
    DEFER
}

void TorusCard::recompute() {
    // Make sure sliders exist
    if (!m_sliders.contains("major") || !m_sliders.contains("minor"))
        return;

    double major = m_sliders["major"]->value();
    double minor = m_sliders["minor"]->value();

    // Enforce minor < major
    if (minor >= major) {
        // Clamp minor to just below major
        minor = major - 0.001;
        if (minor < 0) minor = 0;
        // Update the slider silently (avoid recursion)
        bool oldBlock = m_sliders["minor"]->blockSignals(true);
        m_sliders["minor"]->setValue(minor);
        m_sliders["minor"]->blockSignals(oldBlock);
    }

    if (minor <= 0 || major <= 0) {
        m_rows["Surface Area :"]->setValue("Invalid input");
        m_rows["Volume       :"]->setValue("Invalid input");
        return;
    }

    if (minor >= major) {
        m_rows["Surface Area :"]->setValue("Error: minor must be < major");
        m_rows["Volume       :"]->setValue("Error: minor must be < major");
        return;
    }

    BigDec bdR(major), bdr(minor);
    m_rows["Surface Area :"]->setValue(BigNum::fmt(4 * BigNum::PI * BigNum::PI * bdR * bdr));
    m_rows["Volume       :"]->setValue(BigNum::fmt(2 * BigNum::PI * BigNum::PI * bdR * bdr * bdr));
}
// -- Ellipsoid -----------------------------------------------------------------
EllipsoidCard::EllipsoidCard(double a, double b, double c, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Ellipsoid", this));
    double mx = std::max({ a,b,c }) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("a", a, 0.1, mx, step);
    addSlider("b", b, 0.1, mx, step);
    addSlider("c", c, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Volume       :", "(4/3)\xcf\x80 abc");
    addResult("Surface \xe2\x89\x88   :", "4\xcf\x80((ab)\xb9\xb6+(ac)\xb9\xb6+(bc)\xb9\xb6)/3)\xb9\x1f\xe2\x81\xb6  [Knud Thomsen]");
    DEFER
}
void EllipsoidCard::recompute() {
    BigDec a(m_sliders["a"]->value()), b(m_sliders["b"]->value()), c(m_sliders["c"]->value());
    // Knud Thomsen approximation: 4π((ab)^p+(ac)^p+(bc)^p)/3)^(1/p), p≈1.6075
    BigDec p("1.6075");
    BigDec sa = 4 * BigNum::PI * BigNum::pow((BigNum::pow(a * b, p) + BigNum::pow(a * c, p) + BigNum::pow(b * c, p)) / 3, BigNum::bd(1) / p);
    m_rows["Volume       :"]->setValue(BigNum::fmt(BigNum::bd(4) / 3 * BigNum::PI * a * b * c));
    m_rows["Surface \xe2\x89\x88   :"]->setValue(BigNum::fmt(sa));
}

// -- Capsule -------------------------------------------------------------------
CapsuleCard::CapsuleCard(double r, double h, QWidget* p) : GeoCard(p) {
    m_layout->addWidget(geoTitle("Capsule", this));
    double mx = std::max(r, h) * 5, step = mx > 50 ? 1.0 : 0.1;
    addSlider("r", r, 0.1, mx, step);
    addSlider("h (cyl)", h, 0.1, mx, step);
    m_layout->addWidget(geoDiv());
    addResult("Surface Area :", "2\xcf\x80r(2r+h)");
    addResult("Volume       :", "  \xcf\x80r\xc2\xb2(h+4r/3)");
    addResult("Total Length :", "h + 2r");
    DEFER
}
void CapsuleCard::recompute() {
    BigDec r(m_sliders["r"]->value()), h(m_sliders["h (cyl)"]->value());
    m_rows["Surface Area :"]->setValue(BigNum::fmt(2 * BigNum::PI * r * (2 * r + h)));
    m_rows["Volume       :"]->setValue(BigNum::fmt(BigNum::PI * r * r * (h + BigNum::bd(4) * r / 3)));
    m_rows["Total Length :"]->setValue(BigNum::fmt(h + 2 * r));
}