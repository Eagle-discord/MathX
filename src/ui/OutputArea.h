#pragma once
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QVariantMap>
#include <shapes/GeoCard.h>
#include "../constants/RunState.h"
#include <boost/multiprecision/cpp_int.hpp>
#include "../constants/Theme.h"
#include <QProgressBar>
#include "MasterLabel.h"
#include "../subSystems/WidgetRegistry.h"
#include "ScrubbableExpression.h"
#include "OutputWidget.h"
#include "CopyableLabel.h"
#include <QPointer>
#include "../constants/ResultTypes.h"
#include "ApproxCard.h"


using boost::multiprecision::cpp_int;

struct Progress {
    QStringList text;
    QString tag;
};

class OutputArea : public QScrollArea {
    Q_OBJECT
public:
    explicit OutputArea(QWidget* parent = nullptr);
    bool progressState();
    // Returns the created input line so callers can wire it up. The widget is
    // owned by the layout; the pointer is a non-owning borrow.
    ScrubbableExpression* addInputLine(const QString& expr);
    void showProgress(int percent, const QString& label);   // 0..100
    void hideProgress();
    // Magnitude preview for a slow factorial. Appears directly below the
    // progress bar, then addResultLine() moves it below the finished result so
    // the estimate and the exact value sit together. Previous cards are kept
    // (they belong to their own result further up) but collapsed.
    void showApproximation(const BigNum::FactorialEstimate& est);
    void addResultLine(const QString& text, ResultType type,
        const QString& copyText = QString(),
        const QString& formula = QString(),
        const QString& originalExpr = QString(),
        const double calcTime = 0);
    void addGeoCard(GeoCard* card, const double calcTime = 0);
    void addSplash();
    void addSeparator();
    void clearAll();

    void addPromptRequest(const QString& paramName);
    void addPromptAnswer(const QString& paramName, const QString& value);
    void addBatchHeading(const QString& text);

    // Single source of truth for result-line colour. Used by addResultLine()
    // and by MainWindow when a scrub/edit recompute changes a result's type
    // (e.g. "ok" -> "err"), so the text colour follows the new type.
    static QString colorForType(ResultType type);

    // Rewrites an existing result widget in place, using the SAME html
    // builder addResultLine() uses (wrapTextByWidth + one span per line +
    // <br> joins + a trailing timing span). MainWindow calls this from its
    // scrub/edit retarget path rather than hand-rolling markup -- doing that
    // by hand is what produced an unwrapped, clipped result line with the
    // timing glued onto the last digit.
    void updateResultLine(CopyableLabel* target, const QString& text,
        ResultType type, double calcTime);
    void setAnimationsEnabled(bool on) { m_animate = on; }

    // Re-render every result already on screen. Used when a display-only
    // preference changes (digit grouping), since result HTML is built at insert
    // time and would otherwise keep the old formatting until replaced.
    void reformatResults();
public slots:
    void addSimplified(const QString& before, const QStringList& after);
    void captureWidget(QWidget* widget);

signals:


    // A scrubbed / inline-edited expression wants a FULL re-evaluation (the
    // live path is handled internally via MathEngine::evalSimple).
    //
    // `target` is the OutputWidget that must be updated IN PLACE. MainWindow
    // submits the job, remembers the target against the job id, and on
    // completion rewrites that widget instead of appending a new result line.
    void expressionRecomputeRequested(const QString& expr, CopyableLabel* target);
    void shapeProjectionRequested(const QString& type, const QMap<QString, double>& params);
    void widgetEditRequested(QWidget* target);

protected:
    void resizeEvent(QResizeEvent* e) override;

private:
    // -- live-editing support ----------------------------------------------
    // addInputLine() records the line it just created here; the next
    // addResultLine() claims it and binds the pair. Cleared on claim, so a
    // result with no preceding input (splash text, batch headings) binds
    // nothing.
    QPointer<ScrubbableExpression> m_pendingInput;

    // Wires expr->result live updates for one pair.
    // Wires live expr->result updates for one input/result pair. `color` is
    // the hex the result was rendered with, so live rebuilds match exactly.
    // `result` is any CopyableLabel-derived result widget: OutputWidget for
    // ok/err/trig/alg/func, ClickableLabel for big, ConversionLabel for conv.
    // Live updates go through the virtual CopyableLabel::setNormalText().
    void bindLivePair(ScrubbableExpression* expr, CopyableLabel* result,
        const ResultType& type, const QString& color);

    QProgressBar* m_progressBar = nullptr;

    // Cards persist in the history beside their result, so two distinct roles:
    //   m_pendingApprox - the one still awaiting its result, to be moved below
    //                     it when it lands (cleared immediately after, or a
    //                     later non-factorial result would drag it along).
    //   m_lastApprox    - the newest card, collapsed when another appears.
    // Both survive hideProgress() on purpose: the estimate outlives the bar.
    // Result labels, so a display preference change can reach them. Only the
    // label plus what updateResultLine() needs; the raw value is read back from
    // the label's own copy text.
    struct ResultEntry {
        QPointer<CopyableLabel> label;
        ResultType type = ResultType::none;
        double calcTime = 0;
    };
    QList<ResultEntry> m_results;
    void rememberResult(CopyableLabel* label, ResultType type, double calcTime);

    QPointer<ApproxCard> m_pendingApprox;
    QPointer<ApproxCard> m_lastApprox;

    QLabel* m_progressLabel = nullptr;
    QWidget* m_container = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QLabel* m_relNotes = nullptr;
    void scrollToBottom();
    template<typename T>
    T* trackLabel(T* label, WidgetRole role) {
        static_assert(std::is_base_of<MasterLabel, T>::value,
            "trackLabel() requires a MasterLabel-derived widget");
        WidgetRegistry::instance().add(label, role);
        connect(label, &MasterLabel::editRequested, this, &OutputArea::widgetEditRequested);
        return label;
    }
    bool m_scrollPending = false;
    bool m_animate = true;
};