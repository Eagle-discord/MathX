#pragma once
#include "MasterLabel.h"
#include "NumberFormat.h"
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QMouseEvent>
#include "../constants/Theme.h"

// -- CopyableLabel -----------------------------------------------------------
// Shared base for every terminal label that supports right-click-to-copy,
// a temporary "Copied!" flash, and an optional "detail" string (a formula,
// typically) that can be revealed via click or double-click. Concrete
// subclasses (OutputWidget, ConversionLabel, ClickableLabel) configure
// behavior once via setupCopyable() + a small set of virtuals, rather than
// each reimplementing the copy/timer/hover state machine from scratch.
class CopyableLabel : public MasterLabel {
    Q_OBJECT
        Q_PROPERTY(QString copyText READ copyText WRITE setCopyText)
        Q_PROPERTY(QString color    READ color    WRITE setColor)
        Q_PROPERTY(bool detailVisible READ detailVisible WRITE setDetailVisible)

public:
    // How a non-empty detail string (formula) is revealed, if at all.
    enum class DetailTrigger {
        None,               // no detail behavior
        LeftClickToggle,    // single left click toggles inline detail
        DoubleClickToggle,  // left double-click toggles inline detail
        DoubleClickPopup,   // left double-click shows detail in a QMessageBox
    };

    // Controls whether/when the hover hint ("Right click to copy") appears.
    enum class HoverHint {
        Never,
        Always,
        WhenCopyable,   // subclass decides per-instance via hoverHintEnabled()
    };

    explicit CopyableLabel(const QString& initialText, QWidget* parent = nullptr)
        : MasterLabel(initialText, parent) {
    }

    // How the detail is joined onto the text when shown. Default brackets it as
    // an aside; "= %1" makes it read as an equality
    // ("3,298,534,883,330 = 3.3 trillion").
    void setDetailFormat(const QString& fmt) { m_detailFormat = fmt; }

    // Refresh an already-attached click detail after a recompute (a scrub
    // changes the value, so "= 3.63 million" must not stay behind).
    //
    // Deliberately a NO-OP unless a click detail was actually attached: the
    // update paths call this blindly, and widgets using left-click for something
    // else (the full-digits expand, a conversion's formula popup) must not have
    // their trigger quietly rewritten.
    void updateClickDetail(const QString& detail) {
        if (m_trigger != DetailTrigger::LeftClickToggle) return;
        m_detail = detail;
        if (detail.isEmpty()) m_detailVisible = false;   // nothing left to show
        refreshDisplay();
    }

    // Attach (or, with an empty detail, remove) a left-click reveal. A narrow
    // public entry point on purpose: setupCopyable() stays protected because it
    // is how a SUBCLASS configures itself wholesale, not how a caller adjusts one
    // aspect. Callable unconditionally from the result paths - passing an empty
    // detail clears any existing one and restores the default trigger, which is
    // how the number-names setting turns the reading off live.
    void setClickDetail(const QString& detail, const QString& format) {
        m_detail = detail;
        if (detail.isEmpty()) {
            m_detailVisible = false;
            if (m_trigger == DetailTrigger::LeftClickToggle)
                m_trigger = DetailTrigger::None;
            refreshDisplay();
            return;
        }
        m_detailFormat = format;
        m_trigger = DetailTrigger::LeftClickToggle;
        m_hoverHint = HoverHint::Always;
        refreshDisplay();
    }

    QString copyText() const { return m_copyText; }
    void setCopyText(const QString& v) { m_copyText = v; }
    QString color() const { return m_color; }
    void setColor(const QString& v) { m_color = v; applyNormalStyle(); refreshDisplay(); }
    bool detailVisible() const { return m_detailVisible; }
    void setDetailVisible(bool v) { m_detailVisible = v; refreshDisplay(); }


    // -- live update --------------------------------------------------------
    // Counterpart to the normalText() hook. Subclasses cache the display string
    // in a private member assigned only in their constructor, so a bare
    // setText() is silently reverted the next time refreshDisplay() runs --
    // which happens on every hover and every copy-flash reset. Anything that
    // changes the displayed text at runtime must go through here.
    //
    // Default is a no-op, so subclasses that don't support live updates are
    // unaffected and stay source-compatible.
    virtual void setNormalText(const QString& text) = 0;

protected:
    // Called once by the concrete subclass constructor to configure behavior.
    void setupCopyable(const QString& copyText, const QString& color,
        const QString& detail, DetailTrigger trigger,
        HoverHint hoverHint, int resetMs = 800)
    {
        m_copyText = copyText;
        m_color = color;
        m_detail = detail;
        m_trigger = trigger;
        m_hoverHint = hoverHint;
        m_resetMs = resetMs;

        m_resetTimer.setSingleShot(true);
        connect(&m_resetTimer, &QTimer::timeout, this, &CopyableLabel::resetCopyState);

        setCursor(Qt::PointingHandCursor);
        applyNormalStyle();
        refreshDisplay();
    }

    // -- Subclass hooks ---------------------------------------------------------
    // The text to display when not copied/hovering/showing detail.
    virtual QString normalText() const = 0;

    // Whether the hover hint should currently apply (only consulted when
    // HoverHint::WhenCopyable). Default: always eligible.
    virtual bool hoverHintEnabled() const { return true; }

    // QSS applied in each visual state. Subclasses with extra style needs
    // (e.g. fixed padding) should override; defaults match the common
    // "background:transparent; color:%1;" pattern used across all three
    // original implementations.
    virtual void applyNormalStyle() {
        setStyleSheet(QString("background:transparent; color:%1;").arg(m_color));
    }
    virtual void applyCopiedStyle() {
        setStyleSheet(QString("background:transparent; color:%1;").arg(Theme::ACCENT()));
    }
    virtual void applyDetailStyle() {
        setStyleSheet(QString("background:transparent; color:%1;").arg(Theme::INFO));
    }

    // Splice `frag` in just before the trailing "Calculated in ..." span, so a
    // revealed detail sits next to the NUMBER rather than after the timing.
    // buildResultHtml appends that span last and it is the only MUTED-coloured
    // one it emits, which is what makes it findable without a marker. Falls back
    // to appending when there is no timing (live-scrub previews omit it).
    QString spliceBeforeTiming(const QString& html, const QString& frag) const {
        const QString needle = QStringLiteral("<span style='color:%1;'>").arg(Theme::MUTED);
        int i = html.lastIndexOf(needle);
        if (i < 0) return html + frag;
        // buildResultHtml terminates every result line with <br>, so splicing
        // straight before the timing span drops the detail onto the NEXT line.
        // Step back over that break so it sits beside the number.
        const QString br = QStringLiteral("<br>");
        if (i >= br.size() && QStringView{ html }.mid(i - br.size(), br.size()) == br)
            i -= br.size();
        return html.left(i) + frag + html.mid(i);
    }

    // Re-renders whatever the current state (normal / detail-shown) dictates.
    // Call this after anything affecting normalText()'s output changes.
    void refreshDisplay() {
        if (m_copied) return; // "Copied!" stays until the timer resets it
        // Every branch below changes the DISPLAYED string -- the detail branch
        // appends a "[hint]" span, so the rendered height can differ from the
        // normal branch's. Subclasses caching geometry (see
        // OutputWidget::sizeHint) must be told, or a hover leaves them showing
        // a stale height.
        onDisplayTextChanged();
        if (m_detailVisible && !m_detail.isEmpty()) {
            setText(spliceBeforeTiming(normalText(),
                QString(" <span style='color:%1;'>%2</span>")
                .arg(Theme::MUTED, m_detailFormat.arg(m_detail))));
            applyDetailStyle();
        }
        else {
            setText(normalText());
            applyNormalStyle();
        }
    }

    // Hook for subclasses that cache size/geometry derived from the displayed
    // text. Default does nothing.
    virtual void onDisplayTextChanged() {}

    // -- Event handling -----------------------------------------------------
    void mousePressEvent(QMouseEvent* e) override {
        if (handleInspectorClick(e)) return;

        if (e->button() == Qt::RightButton && !m_copyText.isEmpty()) {
            copyToClipboard();
            e->accept();
            return;
        }
        if (e->button() == Qt::LeftButton && m_trigger == DetailTrigger::LeftClickToggle
            && !m_detail.isEmpty()) {
            toggleDetail();
            e->accept();
            return;
        }
        MasterLabel::mousePressEvent(e);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (inspectorEnabled()) { MasterLabel::mouseDoubleClickEvent(e); return; }

        if (e->button() == Qt::LeftButton && !m_detail.isEmpty()) {
            if (m_trigger == DetailTrigger::DoubleClickToggle) {
                toggleDetail();
                e->accept();
                return;
            }
            if (m_trigger == DetailTrigger::DoubleClickPopup) {
                showDetailPopup();
                e->accept();
                return;
            }
        }
        MasterLabel::mouseDoubleClickEvent(e);
    }

    void enterEvent(QEnterEvent* e) override {
        if (inspectorEnabled()) { MasterLabel::enterEvent(e); return; }

        const bool hintOn = (m_hoverHint == HoverHint::Always) ||
            (m_hoverHint == HoverHint::WhenCopyable && hoverHintEnabled());
        if (!m_copied && hintOn) {
            QString hint = normalText();
            // A very faint '=' where the detail will appear: it shows the line is
            // clickable without asserting anything about the value, and it lands
            // exactly where the revealed text goes so the click feels continuous.
            if (!m_detail.isEmpty() && !m_detailVisible
                && m_trigger == DetailTrigger::LeftClickToggle) {
                hint = spliceBeforeTiming(hint,
                    QString(" <span style='color:%1;'>=</span>").arg(Theme::DIM));
            }
            hint += QString(
                "<span style='color:%1;font-size:10pt;'> &nbsp;&#10064; Right click to copy</span>"
            ).arg(Theme::MUTED);
            setText(hint);
        }
        MasterLabel::enterEvent(e);
    }

    void leaveEvent(QEvent* e) override {
        if (inspectorEnabled()) { MasterLabel::leaveEvent(e); return; }
        if (!m_copied) refreshDisplay();
        MasterLabel::leaveEvent(e);
    }

private:
    void toggleDetail() {
        m_detailVisible = !m_detailVisible;
        refreshDisplay();
    }
    void showDetailPopup(); // implemented in .cpp � needs QMessageBox

    void copyToClipboard() {
        // Group at COPY time, not at construction: what lands on the clipboard
        // then matches what is on screen, without paying to format a result the
        // user never copies (a factorial's copy text is millions of digits).
        // m_copyText itself stays raw, so copyText() keeps returning the value.
        //
        // Note this means a pasted-back result no longer parses. `ans` is the
        // in-app way to reuse a result, and separators cannot be stripped on
        // input because comma is also the argument separator: gcd(123,456) is
        // indistinguishable from the number 123,456.
        QApplication::clipboard()->setText(NumberFormat::groupNumbers(m_copyText));
        m_copied = true;
        setText("Copied!");
        applyCopiedStyle();
        m_resetTimer.start(m_resetMs);
    }
    void resetCopyState() {
        m_copied = false;
        refreshDisplay();
    }

    QString m_copyText, m_color, m_detail;
    QString m_detailFormat = QStringLiteral("[%1]");
    DetailTrigger m_trigger = DetailTrigger::None;
    HoverHint     m_hoverHint = HoverHint::Never;
    int    m_resetMs = 800;
    bool m_copied = false;
    bool m_detailVisible = false;
    QTimer m_resetTimer;
};