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
#include <QPainter>
#include "SliderRow.h"
#include "ResultRow.h"


class GeoCard : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Full, PropertiesOnly };
    explicit GeoCard(QWidget* parent = nullptr, Mode mode = Mode::Full);


    // Builds the formatted copy text:
    //   Line 1:  shape title + prompt (e.g. "Sphere")
    //   Section: slider name = value  (e.g. "r = 5")
    //   Section: key : value pairs    (e.g. "Volume : 523.59")
    QString buildCopyText() const;

    void setTitle(const QString& title) { m_title = title; }
    void applyParams(const QMap<QString, double>& params) {
        if (m_mode == Mode::Full) {
            for (auto it = params.begin(); it != params.end(); ++it)
                if (m_sliders.contains(it.key()))
                    m_sliders[it.key()]->setValue(it.value());
        }
        else {
            m_paramValues = params;
            recompute();
        }
    }
    void setPropertiesOnlyMode() {
        m_propertiesOnly = true;
        // hide everything except result rows
        if (m_copyBtn)   m_copyBtn->hide();
        if (m_toggleBtn) m_toggleBtn->hide();
        // hide sliders
        for (auto* s : m_sliders) s->hide();
        // hide dividers and title - they're direct children of m_body
        // walk m_layout and hide non-ResultRow widgets
        for (int i = 0; i < m_layout->count(); ++i) {
            QWidget* w = m_layout->itemAt(i)->widget();
            if (w && !qobject_cast<ResultRow*>(w))
                w->hide();
        }
        setStyleSheet("background: transparent; border: none;");
    }

   /* void applyParams(const QMap<QString, double>& params) {
        for (auto it = params.begin(); it != params.end(); ++it)
            if (m_sliders.contains(it.key()))
                m_sliders[it.key()]->setValue(it.value());
    }
    */
signals:
    void showProjection(const QString& type, const QMap<QString, double>& params);
    void showShapeProjection(const QString& type, const QMap<QString, double>& params);
    // Re-emitted from a ResultRow when its formula is toggled (hiding the
    // formula stops any running walkthrough).
    void formulaActivated(const QString& rowKey, bool visible);
    // Re-emitted when the user clicks ON a visible formula: play the
    // walkthrough animation in the 3D viewer.
    void formulaClicked(const QString& rowKey);

protected:
    SliderRow* addSlider(const QString& name, double val,
        double minV, double maxV, double step = 0.5);
    ResultRow* addResult(const QString& key, const QString& formula);
    virtual void recompute() = 0;
    double param(const QString& name) const {
        if (m_mode == Mode::PropertiesOnly)
            return m_paramValues.value(name);
        return m_sliders.contains(name) ? m_sliders[name]->value() : 0.0;
    }
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
    void buildPropertiesFrame();

    QPushButton* m_copyBtn = nullptr;
    QPushButton* m_toggleBtn = nullptr;    
    Mode m_mode = Mode::Full;
    QMap<QString, double> m_paramValues;
    bool m_propertiesOnly = false;
};

// Convenience helpers used by all shape cards
QFrame* geoDiv();
QLabel* geoTitle(const QString& text, GeoCard* card = nullptr);
QLabel* geoHint();
double  geoAutoMax(double v);
double  geoAutoStep(double v);