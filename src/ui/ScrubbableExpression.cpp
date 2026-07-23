#include "ScrubbableExpression.h"
#include <QApplication>
#include <QLineEdit>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QFontMetrics>
#include <QTimer>
#include <QDebug>
#include <QCursor>
#include <QGuiApplication>
#include <QScreen>
#include <cmath>
#include <limits>

// ---------------------------------------------------------------------------
ScrubbableExpression::ScrubbableExpression(const QString& expression, QWidget* parent)
    : MasterLabel(expression, parent)
    , m_expression(expression)
{
    m_tokens.retokenize(m_expression);

    // text() is ALWAYS exactly the expression, in plain-text layout. The ">>"
    // prompt is painted, not concatenated, so character 0 of the expression
    // still starts at contentsRect().left() and our offsets stay valid.
    setTextFormat(Qt::PlainText);
    setText(m_expression);

    // Single-line: scrubbing maths assumes one row of glyphs.
    setWordWrap(false);

    // Drives the hover highlight (we need moves with no button held).
    setMouseTracking(true);

    m_prefixColor = QColor(Theme::MUTED);
    applyPrefixMargin();

    // Fail loudly rather than silently. A ScrubbableExpression with nothing
    // connected to expressionEdited drags beautifully and recomputes nothing -
    // which is indistinguishable from a broken evaluator. Check on the next
    // event-loop turn, by which point the caller has had its chance to connect.
    QTimer::singleShot(0, this, [this] {
        if (receivers(SIGNAL(expressionEdited(QString, bool))) == 0) {
            qWarning() << "[scrub] nothing is connected to expressionEdited for"
                << m_expression
                << "-- dragging will change the text but never recompute."
                << "Did OutputArea::addResultLine() call bindLivePair()?";
        }
        });
}

// Reserve space on the left for the painted prefix.
void ScrubbableExpression::applyPrefixMargin() {
    QMargins m = contentsMargins();
    const int w = m_prefix.isEmpty() ? 0 : fontMetrics().horizontalAdvance(m_prefix);
    setContentsMargins(w, m.top(), m.right(), m.bottom());
    updateGeometry();
}

void ScrubbableExpression::setPrefix(const QString& p) {
    if (m_prefix == p) return;
    m_prefix = p;
    applyPrefixMargin();
    update();
}

void ScrubbableExpression::setScrubEnabled(bool on) {
    m_scrubEnabled = on;
    if (!on) clearHighlight();
}

// ---------------------------------------------------------------------------
//  Geometry
// ---------------------------------------------------------------------------
int ScrubbableExpression::textRelativeX(int widgetX) const {
    // contentsRect() already excludes the prefix margin, so its left edge is
    // exactly where the expression's first character is drawn.
    return widgetX - contentsRect().left();
}

bool ScrubbableExpression::textWraps() const {
    if (!wordWrap()) return false;                 // we disable it, but be safe
    const int avail = contentsRect().width();
    if (avail <= 0) return false;
    return fontMetrics().horizontalAdvance(m_expression) > avail;
}

int ScrubbableExpression::tokenAt(int widgetX) const {
    if (!m_scrubEnabled || m_tokens.isEmpty()) return -1;
    // MasterLabel's contract: in inspector mode the click belongs to the
    // inspector. Don't advertise affordances that won't fire.
    if (inspectorEnabled()) return -1;
    if (textWraps()) return -1;
    return m_tokens.hitTest(textRelativeX(widgetX), fontMetrics(), 20);
}

// Character index whose glyph boundary is nearest a widget-space x. Used to put
// the editor's caret exactly where the user clicked, instead of selecting all.
int ScrubbableExpression::charAt(int widgetX) const {
    const int rel = textRelativeX(widgetX);
    if (rel <= 0) return 0;

    const QFontMetrics fm = fontMetrics();
    int best = 0, bestDist = std::numeric_limits<int>::max();
    for (int i = 0; i <= m_expression.size(); ++i) {
        const int x = fm.horizontalAdvance(m_expression.left(i));
        const int d = std::abs(x - rel);
        if (d < bestDist) { bestDist = d; best = i; }
        if (x > rel + fm.averageCharWidth()) break;   // past it, stop
    }
    return best;
}

void ScrubbableExpression::clearHighlight() {
    if (m_hoverToken != -1) {
        m_hoverToken = -1;
        unsetCursor();
        update();
    }
}

// ---------------------------------------------------------------------------
//  Public API
// ---------------------------------------------------------------------------
void ScrubbableExpression::setExpression(const QString& expr) {
    if (m_expression == expr) return;
    m_expression = expr;
    m_tokens.retokenize(m_expression);
    setText(m_expression);          // plain, always
    m_hoverToken = -1;
    updateGeometry();
    update();
}

// ---------------------------------------------------------------------------
//  Infinite scrub + acceleration
// ---------------------------------------------------------------------------
//  Measuring dx from the press position (the obvious approach) stalls the moment
//  the pointer reaches a screen edge: the cursor stops moving, so dx stops
//  growing, so the value freezes mid-drag.
//
//  Instead we accumulate per-event deltas and warp the cursor back to an anchor
//  after each one, so it never actually reaches an edge. The pointer is hidden
//  for the duration, which is what Blender and Figma do for number drags.
//
//  The cost is that the gesture is no longer idempotent. With acceleration, a
//  fast drag right followed by a slow drag left over the same pixel distance
//  does NOT return to the starting value. Escape cancels the drag for exactly
//  this reason -- see cancelDrag().
// ---------------------------------------------------------------------------

// Speed-sensitive multiplier. Deliberately EXACTLY 1.0 below the threshold, so
// slow drags stay pixel-precise; a 1px nudge always moves exactly one step.
double ScrubbableExpression::accelerate(int rawDx) const {
    const double v = std::abs(double(rawDx));
    if (v <= m_accelThreshold) return 1.0;
    const double f = std::pow(v / m_accelThreshold, m_accelExponent);
    return std::min(f, m_accelCap);
}

void ScrubbableExpression::beginInfiniteDrag(const QPoint& globalPos) {
    if (!m_infiniteScrub) return;
    m_anchorGlobal = globalPos;
    m_accumPx = 0.0;
    m_ignoreNextMove = false;
    if (!m_cursorHidden) {
        QGuiApplication::setOverrideCursor(Qt::BlankCursor);
        m_cursorHidden = true;
    }
    grabMouse();   // keep receiving moves even outside our own geometry
}

void ScrubbableExpression::endInfiniteDrag() {
    if (m_cursorHidden) {
        QGuiApplication::restoreOverrideCursor();
        m_cursorHidden = false;
        // Put the pointer back where the user pressed, not wherever the last
        // warp left it.
        QCursor::setPos(m_anchorGlobal);
    }
    if (mouseGrabber() == this) releaseMouse();
    m_accumPx = 0.0;
    m_ignoreNextMove = false;
}

// ---------------------------------------------------------------------------
//  Mouse
// ---------------------------------------------------------------------------
void ScrubbableExpression::mousePressEvent(QMouseEvent* e) {
    // Inspector wins, per MasterLabel's documented contract.
    if (inspectorEnabled()) {
        MasterLabel::mousePressEvent(e);
        return;
    }

    // Ctrl+Left => inline edit. Checked before scrub so it can't be mistaken
    // for the start of a drag.
    if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ControlModifier)) {
        beginEdit(charAt(int(e->position().x())));
        e->accept();
        return;
    }

    if (e->button() == Qt::LeftButton && m_scrubEnabled) {
        const int t = tokenAt(int(e->position().x()));
        if (t >= 0) {
            // Record a *potential* drag without claiming the event. If the
            // pointer never crosses startDragDistance(), the press and release
            // must pass through untouched.
            m_dragToken = t;
            m_dragActive = false;
            m_pressPos = e->pos();
            m_anchorGlobal = e->globalPosition().toPoint();
            m_accumPx = 0.0;
            m_dragStartValue = m_tokens.tokens()[t].value;
            m_exprBeforeDrag = m_expression;
            setFocus(Qt::MouseFocusReason);   // so Escape reaches keyPressEvent
        }
    }

    MasterLabel::mousePressEvent(e);
}

void ScrubbableExpression::mouseMoveEvent(QMouseEvent* e) {
    // -- active scrub -------------------------------------------------------
    if (m_dragToken >= 0 && (e->buttons() & Qt::LeftButton)) {

        // QCursor::setPos() synthesises a move event. Swallow it, or the warp
        // feeds itself and the value runs away.
        if (m_ignoreNextMove) {
            m_ignoreNextMove = false;
            e->accept();
            return;
        }

        const QPoint gp = e->globalPosition().toPoint();

        // Before the threshold is crossed this is still a click, so measure from
        // the press point (cheap, and lets a stray pixel pass through).
        if (!m_dragActive) {
            const int dx = e->pos().x() - m_pressPos.x();
            if (std::abs(dx) < QApplication::startDragDistance()) {
                MasterLabel::mouseMoveEvent(e);
                return;
            }
            m_dragActive = true;
            m_accumPx = 0.0;
            beginInfiniteDrag(m_anchorGlobal);
            emit scrubStarted();
        }

        // Per-event delta, measured in GLOBAL coords: the cursor gets warped
        // back to the anchor each frame, so widget-local coords are useless.
        int rawDx = gp.x() - m_anchorGlobal.x();

        // One bad event (alt-tab, a warp race, a display hotplug) must not fling
        // the value across the number line.
        rawDx = qBound(-kMaxRawDx, rawDx, kMaxRawDx);

        m_accumPx += double(rawDx) * accelerate(rawDx);

        // Pin the pointer so the gesture never runs out of screen.
        if (m_infiniteScrub && rawDx != 0) {
            m_ignoreNextMove = true;
            QCursor::setPos(m_anchorGlobal);
        }

        // Always recompute from the ORIGINAL pre-drag string: token offsets stay
        // valid even as the rendered number changes width.
        ExprTokens base(m_exprBeforeDrag);
        if (m_dragToken >= base.count()) { MasterLabel::mouseMoveEvent(e); return; }

        const double step = base.stepFor(m_dragToken);
        const double steps = m_accumPx / m_pixelsPerStep;
        const double scale = (e->modifiers() & Qt::ShiftModifier) ? 0.1 : 1.0;

        double newValue = m_dragStartValue + std::round(steps) * step * scale;

        // Snap away float crumbs (0.30000000000000004 -> 0.3).
        const double snap = step * scale;
        if (snap > 0) newValue = std::round(newValue / snap) * snap;

        const QString updated = base.withValueAt(m_dragToken, newValue);
        if (updated != m_expression) {
            m_expression = updated;
            m_tokens.retokenize(m_expression);
            setText(m_expression);
            updateGeometry();
            update();
            emit expressionEdited(m_expression, /*live=*/true);
        }
        e->accept();
        return;
    }

    // -- passive hover ------------------------------------------------------
    if (m_scrubEnabled && !isEditing()) {
        const int t = tokenAt(int(e->position().x()));
        if (t != m_hoverToken) {
            m_hoverToken = t;
            if (t >= 0) setCursor(Qt::SplitHCursor);
            else        unsetCursor();
            update();
        }
    }

    MasterLabel::mouseMoveEvent(e);
}

void ScrubbableExpression::mouseReleaseEvent(QMouseEvent* e) {
    if (m_dragActive) {
        endInfiniteDrag();      // unhide + unwarp + release grab
        // Swallow the release: this was a scrub, not a click.
        m_dragActive = false;
        m_dragToken = -1;
        emit scrubFinished(m_expression);
        emit expressionEdited(m_expression, /*live=*/false);
        e->accept();
        return;
    }

    m_dragToken = -1;
    MasterLabel::mouseReleaseEvent(e);
}

// -- cancelDrag ---------------------------------------------------------------
// Acceleration makes a drag non-idempotent: dragging fast to the right and then
// slowly back the same physical distance does NOT restore the original value.
// So an explicit undo is required. Escape restores the pre-drag expression and
// tells the owner to recompute it.
void ScrubbableExpression::cancelDrag() {
    if (!m_dragActive) return;

    endInfiniteDrag();
    m_dragActive = false;
    m_dragToken = -1;

    setExpression(m_exprBeforeDrag);
    emit scrubFinished(m_expression);
    emit expressionEdited(m_expression, /*live=*/false);
}

void ScrubbableExpression::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape && m_dragActive) {
        cancelDrag();
        e->accept();
        return;
    }
    MasterLabel::keyPressEvent(e);
}

// -- mouseDoubleClickEvent ----------------------------------------------------
// Opens the inline editor for precision edits, caret at the clicked character.
//
// Double-click is free on this widget: we derive from MasterLabel, not
// CopyableLabel, so there is no copy-on-click or formula popout to collide with.
// Single click is deliberately NOT used -- it would fire on every scrub that
// never crossed the drag threshold.
void ScrubbableExpression::mouseDoubleClickEvent(QMouseEvent* e) {
    if (inspectorEnabled()) { MasterLabel::mouseDoubleClickEvent(e); return; }

    if (e->button() == Qt::LeftButton) {
        // The first press of the double-click sequence armed a potential scrub.
        // Disarm it, or the release will look like the end of a drag.
        m_dragToken = -1;
        m_dragActive = false;

        beginEdit(charAt(int(e->position().x())));
        e->accept();
        return;
    }
    MasterLabel::mouseDoubleClickEvent(e);
}

void ScrubbableExpression::leaveEvent(QEvent* e) {
    clearHighlight();
    MasterLabel::leaveEvent(e);
}

// ---------------------------------------------------------------------------
//  Paint: ">>" prefix + hovered-token underline
// ---------------------------------------------------------------------------
void ScrubbableExpression::paintEvent(QPaintEvent* e) {
    // While the inline editor is open it renders the expression itself. The
    // editor is transparent and sits directly over the label, so if the label
    // ALSO painted its text the two would overlap and read as a doubled,
    // bold-looking line. Suppress the label's text while editing; the prefix
    // (drawn below) stays visible.
    if (!isEditing())
        MasterLabel::paintEvent(e);     // draws the bare expression

    const QRect cr = contentsRect();
    const QFontMetrics fm = fontMetrics();

    const int t = (m_dragToken >= 0 ? m_dragToken : m_hoverToken);
    const bool drawUnderline = (t >= 0) && !isEditing() && !textWraps();

    if (m_prefix.isEmpty() && !drawUnderline) return;

    QPainter p(this);
    p.setFont(font());

    // -- prefix: painted, never concatenated into text() --------------------
    if (!m_prefix.isEmpty()) {
        p.setPen(m_prefixColor);
        p.drawText(QRect(0, cr.top(), cr.left(), cr.height()),
            Qt::AlignLeft | Qt::AlignVCenter, m_prefix);
    }

    if (!drawUnderline) return;

    int a, b;
    if (!m_tokens.tokenPixelSpan(t, fm, a, b)) return;

    const int x0 = cr.left() + a;
    const int x1 = cr.left() + b;
    const int y = cr.top() + fm.ascent() + 2;

    QPen pen(palette().color(QPalette::WindowText));
    pen.setWidth(m_dragToken >= 0 ? 2 : 1);
    pen.setStyle(m_dragToken >= 0 ? Qt::SolidLine : Qt::DotLine);
    p.setPen(pen);
    p.drawLine(x0, y, x1, y);
}

// ---------------------------------------------------------------------------
//  Inline editing
// ---------------------------------------------------------------------------
void ScrubbableExpression::beginEdit(int caretChar) {
    if (!m_editor.isNull()) return;

    // A drag may still be armed if we got here from Ctrl+click.
    if (m_dragActive) endInfiniteDrag();
    m_dragActive = false;
    m_dragToken = -1;

    clearHighlight();
    m_exprBeforeEdit = m_expression;

    auto* ed = new QLineEdit(m_expression, this);
    m_editor = ed;

    ed->setFont(font());
    ed->setFrame(false);
    ed->setGeometry(contentsRect());        // sits exactly over the expression
    // Opaque background (matching the output area) so the editor fully covers
    // the label's text underneath - a transparent editor would let the label
    // text show through and read as a doubled line. paintEvent also suppresses
    // the label text while editing; the two together make it robust.
    ed->setStyleSheet(QString(
        "QLineEdit{background:%1; border:none; color:%2;}"
    ).arg(C_OUT, palette().color(QPalette::WindowText).name()));

    // editingFinished covers BOTH Return and focus-out, our two commit paths.
    // We deliberately do NOT also connect returnPressed, or Return would fire
    // the handler twice. Escape emits nothing, so it goes via eventFilter().
    connect(ed, &QLineEdit::editingFinished, this, &ScrubbableExpression::commitEdit);
    ed->installEventFilter(this);

    ed->show();
    ed->setFocus();
    if (caretChar >= 0 && caretChar <= m_expression.size())
        ed->setCursorPosition(caretChar);   // caret where the user clicked
    else
        ed->selectAll();                    // Ctrl+click / programmatic: select all
    update();
}

bool ScrubbableExpression::eventFilter(QObject* obj, QEvent* ev) {
    if (!m_editor.isNull() && obj == m_editor && ev->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(ev);
        if (ke->key() == Qt::Key_Escape) {
            cancelEdit();
            return true;                    // consume
        }
    }
    return MasterLabel::eventFilter(obj, ev);
}

void ScrubbableExpression::commitEdit() {
    if (m_editor.isNull()) return;

    // Detach BEFORE hide(): hiding causes focus-out, which fires
    // editingFinished again. Clearing m_editor makes that re-entry a no-op.
    QLineEdit* ed = m_editor;
    m_editor.clear();
    ed->disconnect(this);
    ed->removeEventFilter(this);

    const QString newExpr = ed->text().trimmed();
    ed->hide();
    ed->deleteLater();

    if (!newExpr.isEmpty() && newExpr != m_exprBeforeEdit) {
        setExpression(newExpr);
        emit expressionEdited(m_expression, /*live=*/false);
    }
    else {
        setExpression(m_exprBeforeEdit);
    }
    update();
}

void ScrubbableExpression::cancelEdit() {
    if (m_editor.isNull()) return;

    // Same ordering rule as commitEdit(): detach first, or the focus-out from
    // hide() would fire editingFinished and wrongly KEEP the typed text.
    QLineEdit* ed = m_editor;
    m_editor.clear();
    ed->disconnect(this);
    ed->removeEventFilter(this);

    ed->hide();
    ed->deleteLater();

    setExpression(m_exprBeforeEdit);
    update();
}