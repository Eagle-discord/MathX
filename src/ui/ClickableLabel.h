#pragma once
#include "CopyableLabel.h"
#include <QRegularExpression>

// -- ClickableLabel ------------------------------------------------------------
// Result label that shows "Copy" on hover and copies plain text on right
// click. Left double-click shows the conversion formula (if any) in a popup.
// Now a thin CopyableLabel subclass — all copy/hover/timer logic lives there.
class ClickableLabel : public CopyableLabel {
public:
    ClickableLabel(const QString& html, const QString& plainText,
        const QFont& font, const QString& color,
        const QString& extraStyle = "", QWidget* parent = nullptr,
        const QString& formula = QString())
        : CopyableLabel(html, parent), m_html(html), m_extraStyle(extraStyle)
    {
        setTextFormat(Qt::RichText);
        setFont(font);
        // wordWrap(true), despite buildResultHtml already inserting <br>s at the
        // wrap width. Two reasons:
        //   1) QLabel::minimumSizeHint() for a NON-wrapped label is its full
        //      natural width. One long result therefore forced the whole scroll
        //      container wider than the viewport, dragging geo cards and their
        //      buttons off the right edge -- unreachable, since horizontal
        //      scrolling is off.
        //   2) The <br>s are computed from the viewport width AT INSERT TIME and
        //      never recomputed. Shrink the window and they are stale. Wrap is
        //      the safety net that keeps stale breaks from overflowing.
        // The <br>s still do the real work; this just stops the label lying about
        // how narrow it can be.
        setWordWrap(true);

        // Original showed the hover hint unconditionally (no type filtering)
        // and popped a QMessageBox with the formula on left double-click.
        setupCopyable(plainText, color, formula,
            DetailTrigger::DoubleClickPopup, HoverHint::Always);
    }
    void setNormalText(const QString& html) override {
        m_html = html;
        refreshDisplay();
    }


protected:
    QString normalText() const override { return m_html; }

    void applyNormalStyle() override {
        setStyleSheet("background:transparent;" + m_extraStyle);
    }
    void applyCopiedStyle() override {
        // Original swapped both text ("Copied!") and background pill style —
        // preserve the highlighted background on copy.
        setStyleSheet(QString("background:%1;border-radius:3px;%2")
            .arg(Theme::dimAccent(), m_extraStyle));
    }

private:
    QString m_html, m_extraStyle;
};

static ClickableLabel* makeResultLbl(const QString& html, const QString& plainText,
    const QString& color, const QFont& font,
    const QString& extraStyle = "",
    const QString& formula = "")
{
    return new ClickableLabel(html, plainText, font, color, extraStyle, nullptr, formula);
}