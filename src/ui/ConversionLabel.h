#pragma once
#include "CopyableLabel.h"
#include <QRegularExpression>

class ConversionLabel : public CopyableLabel {
    Q_OBJECT
        Q_PROPERTY(QString result READ result WRITE setResultProp)

public:
    ConversionLabel(const QString& resultNum, const QString& plainText,
        const QString& formulaStr, const QFont& font,
        const QString& color, const QString& extraStyle = "")
        : CopyableLabel(resultNum),
        m_result(resultNum),
        m_plainText(plainText),          // <- given, not guessed
        m_extraStyle(extraStyle)
   
    {
        setFont(font);
        setWordWrap(true);
        setTextFormat(Qt::RichText);

        // Original toggled the formula inline on a plain left click (not a
        // double click, and no right-click conflict since right click is
        // reserved for copy) — LeftClickToggle preserves that exactly.
        setupCopyable(m_plainText, color, formulaStr,
            DetailTrigger::LeftClickToggle, HoverHint::Never);
    }

    QString result() const { return m_result; }
    void setResultProp(const QString& v) {
        m_result = v;
        m_plainText = stripHtml(v);
        setCopyText(m_plainText);
        refreshDisplay();
    }

    // Live-update hook (see CopyableLabel::setNormalText). We already have a
    // setter with exactly these semantics — reuse it rather than duplicate.
    void setNormalText(const QString& html) override { setResultProp(html); }
    

protected:
    QString normalText() const override { return m_result; }

    void applyNormalStyle() override {
        setStyleSheet(QString("background:transparent; color:%1; %2; padding-left:22px;")
            .arg(color(), m_extraStyle));
    }
    void applyCopiedStyle() override {
        // Original ConversionLabel didn't recolor on copy, just swapped text
        // to "Copied!" and left the stylesheet as-is — preserve that.
        // (No style change here; text swap is handled by CopyableLabel.)
    }
    void applyDetailStyle() override {
        setStyleSheet(QString("background:transparent; color:%1; %2; padding-left:22px;")
            .arg(color(), m_extraStyle));
        // Detail color previously came from the inline <span style='color:MUTED'>
        // wrapping the formula text, not a stylesheet change — CopyableLabel's
        // refreshDisplay() already wraps the detail in a MUTED span, so the
        // base label style stays the same as normal here.
    }

private:
    static QString stripHtml(const QString& s) {
        QString plain = s;
        plain.replace(QRegularExpression("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");
        plain.remove(QRegularExpression("<[^>]*>"));
        return plain.trimmed();
    }

    QString m_result, m_plainText, m_extraStyle;
};