#pragma once
#include "GeoCard.h"

class CircleCard : public GeoCard {  public: explicit CircleCard(double r, QWidget* p = nullptr);                    protected: void recompute() override; };
class EllipseCard : public GeoCard {  public: EllipseCard(double a, double b, QWidget* p = nullptr);                  protected: void recompute() override; };
class TriangleCard : public GeoCard {  public: TriangleCard(double a, double b, double c, QWidget* p = nullptr);       protected: void recompute() override; };
class RightTriCard : public GeoCard {  public: RightTriCard(double a, double b, QWidget* p = nullptr);                 protected: void recompute() override; };
class RectCard : public GeoCard {  public: RectCard(double w, double h, QWidget* p = nullptr);                     protected: void recompute() override; };
class SquareCard : public GeoCard {  public: explicit SquareCard(double s, QWidget* p = nullptr);                    protected: void recompute() override; };
class ParallelCard : public GeoCard {  public: ParallelCard(double b, double h, double s, QWidget* p = nullptr);       protected: void recompute() override; };
class TrapezoidCard : public GeoCard {  public: TrapezoidCard(double a, double b, double h, QWidget* p = nullptr);      protected: void recompute() override; };
class RhombusCard : public GeoCard {  public: RhombusCard(double d1, double d2, QWidget* p = nullptr);                protected: void recompute() override; };
class RegPolyCard : public GeoCard {  public: RegPolyCard(int n, double s, QWidget* p = nullptr);                     protected: void recompute() override; };
class SectorCard : public GeoCard {  public: SectorCard(double r, double deg, QWidget* p = nullptr);                 protected: void recompute() override; };
class AnnulusCard : public GeoCard {  public: AnnulusCard(double R, double r, QWidget* p = nullptr);                  protected: void recompute() override; };