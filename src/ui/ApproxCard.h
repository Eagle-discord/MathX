#pragma once
#include <QWidget>
#include <QVector>
#include <QPointer>
#include <QElapsedTimer>
#include <render/MathText.h>
#include <math/BigNum.h>

class QLabel;
class QTimer;

// ---------------------------------------------------------------------------
//  ApproxCard - the "how big is this going to be" preview for a slow factorial.
//
//  Shows, under the progress bar:
//
//      1000000! ≈ 8.2639 × 10^5565708  (5,565,709 digits)
//      obtained using Stirling's approximation  What?
//
//  The formula line is drawn with MathText rather than a QLabel, so the
//  exponent is a real superscript and the whole line can trace itself on
//  (draw-on animation) instead of just appearing. "What?" is italic and
//  clickable; it expands a plain-English account of where Stirling's
//  approximation comes from.
//
//  Lifetime: created below the progress bar when a big factorial starts, then
//  moved below the finished result by OutputArea::addResultLine(), so the
//  estimate and the exact answer end up side by side.
// ---------------------------------------------------------------------------
class ApproxCard : public QWidget {
    Q_OBJECT
public:
    explicit ApproxCard(const BigNum::FactorialEstimate& est, QWidget* parent = nullptr);

    QSize sizeHint() const override;

    // Collapse to just the estimate line - the method comparison and the
    // explainer fold away. Cards persist in the scroll history next to their
    // result, so older ones are collapsed automatically when a new one appears
    // and a session full of factorials stays readable.
    void setExpanded(bool on);
    bool isExpanded() const { return m_expanded; }

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    void relayout();
    void toggleExplanation();
    // Heading, prose and credit form one block: they are the explainer, so they
    // appear and disappear together.
    void setExplanationVisible(bool on);
    bool explanationVisible() const;
    void updateNote();

    bool m_expanded = true;

    BigNum::FactorialEstimate m_est;

    // Laid-out glyphs for the formula line, cached: layout() transforms
    // QPainterPaths and allocates, so it must not run per frame.
    QVector<MathText::Placed> m_glyphs;
    qreal   m_em = 15.0;
    qreal   m_t = 0.0;          // draw-on progress, 0..1
    QTimer* m_anim = nullptr;
    QElapsedTimer m_clock;      // wall clock driving m_t, like the 3D walkthrough

    QLabel* m_heading = nullptr;// "---- Stirling's Approximation ----"
    QLabel* m_compare = nullptr;// Stirling vs log-gamma, side by side
    QLabel* m_note = nullptr;   // agreement verdict + "What?"
    QLabel* m_expl = nullptr;   // the expanded explanation (hidden initially)
    QLabel* m_credit = nullptr; // who discovered it
};
