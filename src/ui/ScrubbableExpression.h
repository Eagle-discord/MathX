#pragma once
//  ScrubbableExpression — the ">> 2 + 3" input line, made interactive.
//
//  Drag any number to change it; Ctrl+click to edit the whole expression
//  inline. Either action re-evaluates and updates the paired result widget.
//
//  WHY IT DERIVES FROM MasterLabel, NOT OutputWidget
//  -------------------------------------------------
//  An earlier draft inherited OutputWidget (-> CopyableLabel -> MasterLabel).
//  That was wrong. CopyableLabel exists to provide copy-on-right-click, a
//  double-click formula popout, and an HTML hover hint. The input line wants
//  none of the three — and all three work by injecting markup into text(),
//  which flips the label to rich text.
//
//  That matters because scrubbing needs a 1:1 character->pixel mapping, and
//  that only holds in plain-text layout. Once QLabel renders rich text,
//  QTextDocument collapses whitespace runs ("2 *  3" -> "2 * 3"), strips
//  leading/trailing space, and applies its own block margin that moves the text
//  origin — so offsets computed with QFontMetrics go stale, even for glyphs
//  sitting before the injected markup.
//
//  Deriving from MasterLabel sidesteps all of it: no CopyableLabel, no HTML,
//  no refreshDisplay() to override. text() is always exactly the expression.
//  We keep MasterLabel so OutputArea::trackLabel() still accepts us (it
//  static_asserts on MasterLabel) and the widget inspector keeps working.
//
//  The ">>" prompt prefix is NOT part of text(). It is painted at x=0 and the
//  expression is inset past it via setContentsMargins(), so character 0 of the
//  expression still begins at contentsRect().left().
//
//  GESTURES
//  --------
//      hover a number        -> dotted underline, SplitHCursor
//      left-drag             -> scrub it (right = up, left = down)
//      Shift + drag          -> 10x finer steps
//      Ctrl + left-click     -> inline edit (transparent QLineEdit overlay)
//      Enter / focus-out     -> commit
//      Esc                   -> cancel
//
//  A press becomes a scrub only after QApplication::startDragDistance(), and a
//  recognised drag swallows its release, so plain clicks pass through cleanly.
//  When the widget inspector is enabled, MasterLabel claims the click and both
//  scrubbing and editing stand down — per MasterLabel's documented contract.

#include "MasterLabel.h"
#include "ExprTokens.h"
#include <QPointer>
#include <QEnterEvent>
#include <QColor>
#include <QSize>

class QLineEdit;

class ScrubbableExpression : public MasterLabel {
    Q_OBJECT
        Q_PROPERTY(QString expression READ expression WRITE setExpression)
        Q_PROPERTY(bool scrubEnabled READ scrubEnabled WRITE setScrubEnabled)
        Q_PROPERTY(double dragSensitivity READ dragSensitivity WRITE setDragSensitivity)

public:
    explicit ScrubbableExpression(const QString& expression,
        QWidget* parent = nullptr);

    QString expression() const { return m_expression; }
    void    setExpression(const QString& expr);

    bool scrubEnabled() const { return m_scrubEnabled; }
    void setScrubEnabled(bool on);

    // Pixels of horizontal drag per one step of the hovered token.
    double dragSensitivity() const { return m_pixelsPerStep; }
    void   setDragSensitivity(double px) { m_pixelsPerStep = (px > 0.5 ? px : 0.5); }

    // The ">>" prompt painted to the left of the expression. Empty hides it.
    QString prefix() const { return m_prefix; }
    void    setPrefix(const QString& p);

    // Colour of the painted prefix.
    void setPrefixColor(const QColor& c) { m_prefixColor = c; update(); }

    bool isEditing() const { return !m_editor.isNull(); }

    // -- infinite scrub -----------------------------------------------------
    // When on, a drag hides the pointer and pins it to the press point, so the
    // gesture never runs out of screen. Pixel deltas accumulate instead of
    // being measured from the press position.
    bool infiniteScrub() const { return m_infiniteScrub; }
    void setInfiniteScrub(bool on) { m_infiniteScrub = on; }

    // Mouse-speed acceleration. Below `threshold` px per move event the drag is
    // exactly 1:1 (so slow drags stay pixel-precise); above it, the delta is
    // multiplied by pow(dx/threshold, exponent), capped at `cap`.
    void setAcceleration(double threshold, double exponent, double cap) {
        m_accelThreshold = qMax(1.0, threshold);
        m_accelExponent = qBound(0.0, exponent, 2.0);
        m_accelCap = qMax(1.0, cap);
    }

signals:
    // Fired continuously while dragging (live=true), and once when a drag ends
    // or an inline edit is committed (live=false). OutputArea evaluates and
    // updates the paired result widget.
    void expressionEdited(const QString& newExpression, bool live);
    void scrubStarted();
    void scrubFinished(const QString& finalExpression);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;

    // Double-click anywhere on the line opens the inline editor, with the caret
    // placed at the character you clicked. Ctrl+click does the same.
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void leaveEvent(QEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

    // Catches Escape inside the inline editor (QLineEdit emits no signal for it).
    bool eventFilter(QObject* obj, QEvent* ev) override;

    // Escape during a scrub cancels it. Acceleration makes a drag non-idempotent
    // (a fast-out/slow-back gesture does NOT return to the original value), so
    // an explicit undo is required, not optional.
    void keyPressEvent(QKeyEvent* e) override;

private:
    // caretChar < 0 => select all. Otherwise place the caret at that index.
    void beginEdit(int caretChar = -1);
    void commitEdit();
    void cancelEdit();
    void cancelDrag();          // Esc during a scrub: restore pre-drag expression
    void clearHighlight();
    void applyPrefixMargin();

    // Single-line only: our pixel maths can't follow a wrapped layout.
    bool textWraps() const;
    int  textRelativeX(int widgetX) const;
    int  tokenAt(int widgetX) const;
    // Character index nearest a widget-space x, for caret placement.
    int  charAt(int widgetX) const;

    ExprTokens m_tokens;
    QString    m_expression;
    QString    m_prefix = QStringLiteral(">> ");
    QColor     m_prefixColor;

    bool   m_scrubEnabled = true;
    int    m_hoverToken = -1;
    int    m_dragToken = -1;
    bool   m_dragActive = false;
    QPoint m_pressPos;
    double m_dragStartValue = 0.0;
    double m_pixelsPerStep = 6.0;
    QString m_exprBeforeDrag;

    // -- infinite scrub / acceleration state --------------------------------
    bool   m_infiniteScrub = true;
    QPoint m_anchorGlobal;          // screen point the cursor is pinned to
    double m_accumPx = 0.0;  // accelerated pixels accumulated this drag
    bool   m_ignoreNextMove = false;// swallow the synthetic move from setPos()
    bool   m_cursorHidden = false;

    double m_accelThreshold = 4.0;  // px/event below which no acceleration
    double m_accelExponent = 0.8;
    double m_accelCap = 8.0;

    // Raw per-event dx above this is treated as a glitch (alt-tab, warp race)
    // and clamped, so one bad event can't fling the value into orbit.
    static constexpr int kMaxRawDx = 200;

    double accelerate(int rawDx) const;
    void   beginInfiniteDrag(const QPoint& globalPos);
    void   endInfiniteDrag();

    QPointer<QLineEdit> m_editor;
    QString m_exprBeforeEdit;
};