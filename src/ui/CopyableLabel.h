#pragma once
#include "MasterLabel.h"
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
            setText(QString("%1 <span style='color:%2;'>[%3]</span>")
                .arg(normalText(), Theme::MUTED, m_detail));
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
            setText(normalText() + QString(
                "<span style='color:%1;font-size:10pt;'> &nbsp;&#10064; Right click to copy</span>"
            ).arg(Theme::MUTED));
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
        QApplication::clipboard()->setText(m_copyText);
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
    DetailTrigger m_trigger = DetailTrigger::None;
    HoverHint     m_hoverHint = HoverHint::Never;
    int    m_resetMs = 800;
    bool m_copied = false;
    bool m_detailVisible = false;
    QTimer m_resetTimer;
};