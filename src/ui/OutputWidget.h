#pragma once
#include "CopyableLabel.h"
#include "../constants/ResultTypes.h"
#include <QResizeEvent>

class OutputWidget : public CopyableLabel {
    Q_OBJECT
        Q_PROPERTY(QString originalExpr READ originalExpr WRITE setOriginalExpr)

public:
    explicit OutputWidget(const QString& displayText,
        const QString& copyText,
        const QString& originalExpr,
        const QString& formula,
        ResultType type,
        const QString& color,
        const QString& extraStyle = "padding-left:22px;",
        QWidget* parent = nullptr);

    QString originalExpr() const { return m_originalExpr; }
    void setOriginalExpr(const QString& v) { m_originalExpr = v; }

    QString displayText() const { return m_displayText; }

    // Enable left-click expand/collapse for a numeric result. Both forms are
    // pre-built HTML; the copy text swaps to match whichever is showing, so a
    // right-click copy always yields the value the user is looking at.
    void setExpandable(const QString& collapsedHtml, const QString& expandedHtml,
        const QString& collapsedCopy, const QString& expandedCopy) {
        m_expandable = true;
        m_collapsedHtml = collapsedHtml;
        m_expandedHtml = expandedHtml;
        m_collapsedCopy = collapsedCopy;
        m_expandedCopy = expandedCopy;
        m_expanded = false;
        setToolTip(QStringLiteral("Left-click to expand / collapse"));
        setCopyText(m_collapsedCopy);
        setNormalText(m_collapsedHtml);
    }


    // Live update hook (see CopyableLabel::setNormalText). refreshDisplay()
    // rebuilds the label from normalText() on every hover and copy-flash reset,
    // so a bare setText() would be silently reverted.
    void setNormalText(const QString& html) override {
        m_displayText = html;
        refreshDisplay();     // invalidates the hint via onDisplayTextChanged()
    }
    ResultType type() const { return m_type; }
    void setType(ResultType v) { m_type = v; }

    // -- Cached size hint ------------------------------------------------------
    // Inserting a line invalidates the QVBoxLayout, which then asks EVERY child
    // for its sizeHint(). At line N that is N queries, so a batch is O(n^2)
    // (BUG-025: frame interval grew +19 ms per line, ~636 ms stalls by line 34).
    //
    // A result line's text never changes after insert except through
    // setNormalText() (scrub / expand-collapse), and its height only depends on
    // the text and the width. So compute once, remember, and invalidate on the
    // two things that can actually change it. QLabel::sizeHint() is itself
    // cached internally, but it is invalidated far more aggressively than we
    // need -- a stylesheet reapply on hover is enough to drop it, and this
    // widget restyles on every hover and copy-flash.
    QSize sizeHint() const override {
        if (!m_hintValid || m_hintWidth != width()) {
            m_hint = CopyableLabel::sizeHint();
            m_hintWidth = width();
            m_hintValid = true;
        }
        return m_hint;
    }

    // NOT overridden to return sizeHint(). An earlier version did, and it made
    // every result line demand its natural (unwrapped) width as a MINIMUM -- so
    // the scroll container could never shrink below the longest line, grew to
    // fit it, and long results ran off the right edge instead of wrapping.
    // QLabel::minimumSizeHint() is deliberately small for a word-wrapped label;
    // that is the behaviour we want and the base class already has it.

protected:
    // Any displayed-text change (normal text, hover hint appended/removed,
    // copy-flash reset) drops the cached height.
    void onDisplayTextChanged() override { m_hintValid = false; }

    // Width changes are the other thing that moves the height (wrapped text
    // reflows). Nothing else needs to drop the cache.
    void resizeEvent(QResizeEvent* e) override {
        if (e->size().width() != e->oldSize().width()) m_hintValid = false;
        CopyableLabel::resizeEvent(e);
    }

signals:
    void expressionChanged(const QString& newExpr, const QString& newResult); // optional

protected:
    QString normalText() const override { return m_displayText; }

    // Left-click toggles expand/collapse for numeric results; everything else
    // (right-click copy, inspector, conv/alg detail) defers to CopyableLabel.
    void mousePressEvent(QMouseEvent* e) override {
        if (m_expandable && !inspectorEnabled() && e->button() == Qt::LeftButton) {
            m_expanded = !m_expanded;
            setCopyText(m_expanded ? m_expandedCopy : m_collapsedCopy);
            setNormalText(m_expanded ? m_expandedHtml : m_collapsedHtml);
            e->accept();
            return;
        }
        CopyableLabel::mousePressEvent(e);
    }

    bool hoverHintEnabled() const override {
        return m_type == ResultType::ok || m_type == ResultType::conv
            || m_type == ResultType::alg || m_type == ResultType::func;
    }

    void applyNormalStyle() override {
        setStyleSheet(QString("background:transparent; color:%1; %2;").arg(color(), m_extraStyle));
    }
    void applyCopiedStyle() override {
        setStyleSheet(QString("background:transparent; color:%1; %2;").arg(Theme::ACCENT(), m_extraStyle));
    }
    void applyDetailStyle() override {
        setStyleSheet(QString("background:transparent; color:%1; %2;").arg(Theme::INFO, m_extraStyle));
    }

private:
    QString m_displayText;
    QString m_originalExpr;
    ResultType m_type = ResultType::none;
    QString m_extraStyle;

    // sizeHint cache -- see sizeHint() above. mutable so the const override can
    // fill it lazily.
    mutable QSize m_hint;
    mutable int   m_hintWidth = -1;
    mutable bool  m_hintValid = false;

    // Expand/collapse state for numeric results (see setExpandable).
    bool m_expandable = false;
    bool m_expanded = false;
    QString m_collapsedHtml, m_expandedHtml, m_collapsedCopy, m_expandedCopy;
};