#pragma once
#include "GeoCard.h"
#include <QMap>
#include <QString>

class CircleCard : public GeoCard {
public:
    explicit CircleCard(double r, QWidget* p = nullptr);
    explicit CircleCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : CircleCard(p.value("r", 1.0), parent) {
    }
protected: void recompute() override;
};

class EllipseCard : public GeoCard {
public:
    EllipseCard(double a, double b, QWidget* p = nullptr);
    explicit EllipseCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : EllipseCard(p.value("a", 2.0), p.value("b", 1.0), parent) {
    }
protected: void recompute() override;
};

class TriangleCard : public GeoCard {
public:
    TriangleCard(double a, double b, double c, QWidget* p = nullptr);
    explicit TriangleCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : TriangleCard(p.value("a", 3.0), p.value("b", 4.0), p.value("c", 5.0), parent) {
    }
protected: void recompute() override;
};

class RightTriCard : public GeoCard {
public:
    RightTriCard(double a, double b, QWidget* p = nullptr);
    explicit RightTriCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : RightTriCard(p.value("a", 3.0), p.value("b", 4.0), parent) {
    }
protected: void recompute() override;
};

class RectCard : public GeoCard {
public:
    RectCard(double w, double h, QWidget* p = nullptr);
    explicit RectCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : RectCard(p.value("w", 2.0), p.value("h", 1.0), parent) {
    }
protected: void recompute() override;
};

class SquareCard : public GeoCard {
public:
    explicit SquareCard(double s, QWidget* p = nullptr);
    explicit SquareCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : SquareCard(p.value("s", 1.0), parent) {
    }
protected: void recompute() override;
};

class ParallelCard : public GeoCard {
public:
    ParallelCard(double b, double h, double s, QWidget* p = nullptr);
    explicit ParallelCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : ParallelCard(p.value("b", 2.0), p.value("h", 1.0), p.value("s", 2.0), parent) {
    }
protected: void recompute() override;
};

class TrapezoidCard : public GeoCard {
public:
    TrapezoidCard(double a, double b, double h, QWidget* p = nullptr);
    explicit TrapezoidCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : TrapezoidCard(p.value("a", 3.0), p.value("b", 5.0), p.value("h", 2.0), parent) {
    }
protected: void recompute() override;
};

class RhombusCard : public GeoCard {
public:
    RhombusCard(double d1, double d2, QWidget* p = nullptr);
    explicit RhombusCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : RhombusCard(p.value("d1", 4.0), p.value("d2", 3.0), parent) {
    }
protected: void recompute() override;
};

class RegPolyCard : public GeoCard {
public:
    RegPolyCard(int n, double s, QWidget* p = nullptr);
    explicit RegPolyCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : RegPolyCard((int)p.value("n", 6.0), p.value("s", 1.0), parent) {
    }
protected: void recompute() override;
};

class SectorCard : public GeoCard {
public:
    SectorCard(double r, double deg, QWidget* p = nullptr);
    explicit SectorCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : SectorCard(p.value("r", 1.0), p.value("deg", 90.0), parent) {
    }
protected: void recompute() override;
};

class AnnulusCard : public GeoCard {
public:
    AnnulusCard(double R, double r, QWidget* p = nullptr);
    explicit AnnulusCard(const QMap<QString, double>& p, QWidget* parent = nullptr)
        : AnnulusCard(p.value("R", 2.0), p.value("r", 1.0), parent) {
    }
protected: void recompute() override;
};