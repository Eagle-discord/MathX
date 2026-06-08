#pragma once
#include "GeoCard.h"
#include <render/OpenGLRenderer.h>

class SphereCard : public GeoCard { public: explicit SphereCard(double r, QWidget* p = nullptr);                    protected: void recompute() override; };

class HemiSphereCard  : public GeoCard {  public: explicit HemiSphereCard(double r, QWidget* p=nullptr);                protected: void recompute() override; };
class CylinderCard    : public GeoCard {  public: CylinderCard(double r, double h, QWidget* p=nullptr);                 protected: void recompute() override; };
class HollowCylCard   : public GeoCard {  public: HollowCylCard(double R, double r, double h, QWidget* p=nullptr);      protected: void recompute() override; };
class ConeCard        : public GeoCard {  public: ConeCard(double r, double h, QWidget* p=nullptr);                     protected: void recompute() override; };
class FrustumCard     : public GeoCard {  public: FrustumCard(double R, double r, double h, QWidget* p=nullptr);        protected: void recompute() override; };
class CuboidCard      : public GeoCard {  public: CuboidCard(double l, double w, double h, QWidget* p=nullptr);         protected: void recompute() override; };
class CubeCard        : public GeoCard {  public: explicit CubeCard(double s, QWidget* p=nullptr);                      protected: void recompute() override; };
class TetraCard       : public GeoCard {  public: explicit TetraCard(double s, QWidget* p=nullptr);                     protected: void recompute() override; };
class OctaCard        : public GeoCard {  public: explicit OctaCard(double s, QWidget* p=nullptr);                      protected: void recompute() override; };
class IcosaCard       : public GeoCard {  public: explicit IcosaCard(double s, QWidget* p=nullptr);                     protected: void recompute() override; };
class DodecaCard      : public GeoCard {  public: explicit DodecaCard(double s, QWidget* p=nullptr);                    protected: void recompute() override; };
class PrismCard       : public GeoCard {  public: PrismCard(int n, double s, double h, QWidget* p=nullptr);             protected: void recompute() override; };
class PyramidCard     : public GeoCard {  public: PyramidCard(double b, double h, QWidget* p=nullptr);                  protected: void recompute() override; };
class TorusCard       : public GeoCard {  public: TorusCard(double R, double r, QWidget* p=nullptr);                    protected: void recompute() override; };
class EllipsoidCard   : public GeoCard {  public: EllipsoidCard(double a, double b, double c, QWidget* p=nullptr);      protected: void recompute() override; };
class CapsuleCard     : public GeoCard {  public: CapsuleCard(double r, double h, QWidget* p=nullptr);                  protected: void recompute() override; };
