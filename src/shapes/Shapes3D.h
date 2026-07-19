#pragma once
#include "GeoCard.h"
#include <QMap>
#include <QString>

class SphereCard : public GeoCard {
public:
    explicit SphereCard(double r, QWidget* p = nullptr);
    explicit SphereCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : SphereCard(p.value("r", 1.0), parent) {
    }
protected: void recompute() override;
};

class HemiSphereCard : public GeoCard {
public:
    explicit HemiSphereCard(double r, QWidget* p = nullptr);
    explicit HemiSphereCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : HemiSphereCard(p.value("r", 1.0), parent) {
    }
protected: void recompute() override;
};

class CylinderCard : public GeoCard {
public:
    CylinderCard(double r, double h, QWidget* p = nullptr);
    explicit CylinderCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : CylinderCard(p.value("r", 0.5), p.value("h", 1.0), parent) {
    }
protected: void recompute() override;
};

class HollowCylCard : public GeoCard {
public:
    HollowCylCard(double R, double r, double h, QWidget* p = nullptr);
    explicit HollowCylCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : HollowCylCard(p.value("R", 2.0), p.value("r", 1.0), p.value("h", 3.0), parent) {
    }
protected: void recompute() override;
};

class ConeCard : public GeoCard {
public:
    ConeCard(double r, double h, QWidget* p = nullptr);
    explicit ConeCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : ConeCard(p.value("r", 1.0), p.value("h", 2.0), parent) {
    }
protected: void recompute() override;
};

class FrustumCard : public GeoCard {
public:
    FrustumCard(double R, double r, double h, QWidget* p = nullptr);
    explicit FrustumCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : FrustumCard(p.value("R", 2.0), p.value("r", 1.0), p.value("h", 3.0), parent) {
    }
protected: void recompute() override;
};

class CuboidCard : public GeoCard {
public:
    CuboidCard(double l, double w, double h, QWidget* p = nullptr);
    explicit CuboidCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : CuboidCard(p.value("l", 2.0), p.value("w", 1.0), p.value("h", 3.0), parent) {
    }
protected: void recompute() override;
};

class CubeCard : public GeoCard {
public:
    explicit CubeCard(double s, QWidget* p = nullptr);
    explicit CubeCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : CubeCard(p.value("s", 1.0), parent) {
    }
protected: void recompute() override;
};

class TetraCard : public GeoCard {
public:
    explicit TetraCard(double s, QWidget* p = nullptr);
    explicit TetraCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : TetraCard(p.value("s", 1.0), parent) {
    }
protected: void recompute() override;
};

class OctaCard : public GeoCard {
public:
    explicit OctaCard(double s, QWidget* p = nullptr);
    explicit OctaCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : OctaCard(p.value("s", 1.0), parent) {
    }
protected: void recompute() override;
};

class IcosaCard : public GeoCard {
public:
    explicit IcosaCard(double s, QWidget* p = nullptr);
    explicit IcosaCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : IcosaCard(p.value("s", 1.0), parent) {
    }
protected: void recompute() override;
};

class DodecaCard : public GeoCard {
public:
    explicit DodecaCard(double s, QWidget* p = nullptr);
    explicit DodecaCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : DodecaCard(p.value("s", 1.0), parent) {
    }
protected: void recompute() override;
};

class PrismCard : public GeoCard {
public:
    PrismCard(int n, double s, double h, QWidget* p = nullptr);
    explicit PrismCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : PrismCard((int)p.value("n", 6.0), p.value("s", 1.0), p.value("h", 2.0), parent) {
    }
protected: void recompute() override;
};

class PyramidCard : public GeoCard {
public:
    PyramidCard(double b, double h, QWidget* p = nullptr);
    explicit PyramidCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : PyramidCard(p.value("b", 2.0), p.value("h", 3.0), parent) {
    }
protected: void recompute() override;
};

class TorusCard : public GeoCard {
public:
    TorusCard(double R, double r, QWidget* p = nullptr);
    explicit TorusCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : TorusCard(p.value("R", 2.0), p.value("r", 0.5), parent) {
    }
protected: void recompute() override;
};

class EllipsoidCard : public GeoCard {
public:
    EllipsoidCard(double a, double b, double c, QWidget* p = nullptr);
    explicit EllipsoidCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : EllipsoidCard(p.value("a", 3.0), p.value("b", 2.0), p.value("c", 1.0), parent) {
    }
protected: void recompute() override;
};

class CapsuleCard : public GeoCard {
public:
    CapsuleCard(double r, double h, QWidget* p = nullptr);
    explicit CapsuleCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : CapsuleCard(p.value("r", 1.0), p.value("h", 2.0), parent) {
    }
protected: void recompute() override;
};