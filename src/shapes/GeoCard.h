#pragma once
#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMap>
#include <QStringList>
#include <QString>
#include <QMouseEvent>
#include <QPushButton>
#include <QRegularExpression>

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
    QString displayText() const;
    void setValue(const QString& v);
    QString value() const;
protected:
    void enterEvent(QEnterEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override; 
   
private:
   
    void toggleFormula();
    QLabel* m_formulaInline = nullptr;
    QLabel* m_keyLbl = nullptr;
    QLabel* m_valLbl = nullptr;
    QLabel* m_formulaLbl = nullptr;
    QString m_formulaString;
    QString  m_currentValue;           // plain text, used for clipboard + restore
    QString m_rawKey;

    bool     m_formulaVisible = false;
    bool     m_copied = false;
};

class GeoCard : public QWidget {
    Q_OBJECT
public:
    explicit GeoCard(QWidget* parent = nullptr);

    // Builds the formatted copy text:
    //   Line 1:  shape title + prompt (e.g. "Sphere")
    //   Section: slider name = value  (e.g. "r = 5")
    //   Section: key : value pairs    (e.g. "Volume : 523.59")
    QString buildCopyText() const;

    void setTitle(const QString& title) { m_title = title; }

signals:
    void showProjection(const QString& type, const QMap<QString, double>& params);
    void showShapeProjection(const QString& type, const QMap<QString, double>& params);

protected:
    SliderRow* addSlider(const QString& name, double val,
        double minV, double maxV, double step = 0.1);
    ResultRow* addResult(const QString& key, const QString& formula);
    virtual void recompute() = 0;

    // Ordered so copy text matches visual order
    QMap<QString, SliderRow*> m_sliders;
    QMap<QString, ResultRow*> m_rows;
    QStringList               m_sliderOrder;  // insertion order of sliders
    QStringList               m_rowOrder;     // insertion order of result rows
    QString                   m_title;        // set by subclass via setTitle()
    QWidget* m_body = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QString m_shapeType;

private:
    void buildFrame();
    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_toggleBtn = nullptr;     
};

// Convenience helpers used by all shape cards
QFrame* geoDiv();
QLabel* geoTitle(const QString& text, GeoCard* card = nullptr);
QLabel* geoHint();
double  geoAutoMax(double v);
double  geoAutoStep(double v);