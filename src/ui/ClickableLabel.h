#include <QLabel>
#include <QString>
#include <QTimer>
#include "..\theme\Theme.h"
#include <QApplication>
#include <QClipboard>
#include "OutputArea.h"

// ── ClickableLabel ────────────────────────────────────────────────────────────
// Result label that shows "Copy" on hover and copies plain text on click.
class ClickableLabel : public QLabel {
public:
    ClickableLabel(const QString& html, const QString& plainText,
        const QFont& font, const QString& color,
        const QString& extraStyle = "", QWidget* parent = nullptr)
        : QLabel(parent)
        , m_html(html)
        , m_plainText(plainText)
        , m_color(color)
        , m_extraStyle(extraStyle)
    {
        setTextFormat(Qt::RichText);
        setFont(font);
        setText(html);
        setStyleSheet("background:transparent;" + extraStyle);
        setWordWrap(true);
        setCursor(Qt::PointingHandCursor);
        m_resetTimer = new QTimer(this);
        m_resetTimer->setSingleShot(true);
        connect(m_resetTimer, &QTimer::timeout, this, [this] { resetToNormal(); });
    }
    /*ClickableLabel(const QString& plainText, const QString& extraStyle) {
        setTextFormat(Qt::RichText);
        setFont(MF(9));
        setText(plainText);
        setStyleSheet("background:transparent;");
        setWordWrap(false);
        setCursor(Qt::PointingHandCursor);
        m_resetTimer = new QTimer(this);
        m_resetTimer->setSingleShot(true);
        connect(m_resetTimer, &QTimer::timeout, this, [this] { resetToNormal(); });
    }*/
protected:
    void enterEvent(QEnterEvent*) override {
        if (m_copied) return;
        setText(m_html + QString("<span style='color:%1;font-size:8pt;'>"
            " &nbsp;&#10064; Copy</span>").arg(C_MUTED));
    }
    void leaveEvent(QEvent*) override {
        if (m_copied) return;
        setText(m_html);
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton) { QLabel::mousePressEvent(e); return; }
        QApplication::clipboard()->setText(m_plainText);
        m_copied = true;
        setText(QString("<span style='color:%1;'>Copied!</span>").arg(C_ACCENT));
        setStyleSheet(QString("background:%1;border-radius:3px;%2").arg(C_DIM, m_extraStyle));
        m_resetTimer->start(1200);
        QLabel::mousePressEvent(e);
    }

private:
    void resetToNormal() {
        m_copied = false;
        setText(m_html);
        setStyleSheet("background:transparent;" + m_extraStyle);
    }
    QString  m_html;
    QString  m_plainText;
    QString  m_color;
    QString  m_extraStyle;
    bool     m_copied = false;
    QTimer* m_resetTimer = nullptr;
};
