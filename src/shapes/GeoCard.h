#pragma once
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMap>
#include <QString>
#include <QMouseEvent>

class SliderRow : public QWidget {
    Q_OBJECT
public:
    SliderRow(const QString& paramName, double initialVal,
        double minVal, double maxVal, double step,
        QWidget* parent = nullptr);
    double value() const;
    void setValue(double v);
signals:
    void valueChanged(double newVal);
private slots:
    void onSliderMoved(int tick);
private:
    void showPopup(double val);
    void hidePopup();
    int toTick(double v) const;
    double fromTick(int t) const;
    QString m_name;
    double m_min, m_max, m_step;
    QLabel* m_nameLabel = nullptr;
    QSlider* m_slider = nullptr;
    QLabel* m_valLabel = nullptr;
    QLabel* m_popup = nullptr;
};

class ResultRow : public QWidget {
    Q_OBJECT
public:
    ResultRow(const QString& key, const QString& formula, QWidget* parent = nullptr);
    void setValue(const QString& v);
    QString value() const;
protected:
    void mousePressEvent(QMouseEvent* e) override;
private:
    void toggleFormula();
    QLabel* m_keyLbl = nullptr;
    QLabel* m_valLbl = nullptr;
    QLabel* m_formulaLbl = nullptr;
    bool m_formulaVisible = false;
};

class GeoCard : public QWidget {
    Q_OBJECT
public:
    explicit GeoCard(QWidget* parent = nullptr);
protected:
    SliderRow* addSlider(const QString& name, double val,
        double minV, double maxV, double step = 0.1);
    ResultRow* addResult(const QString& key, const QString& formula);
    virtual void recompute() = 0;
    QMap<QString, SliderRow*> m_sliders;
    QMap<QString, ResultRow*> m_rows;
    QWidget* m_body = nullptr;
    QVBoxLayout* m_layout = nullptr;
private:
    void buildFrame();
};

// Convenience helpers used by all shape cards
QFrame* geoDiv();
QLabel* geoTitle(const QString& text);
QLabel* geoHint();
double  geoAutoMax(double v);
double  geoAutoStep(double v);