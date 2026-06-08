#include "OutputArea.h"
#include "OutputWidget.h"
#include "Animations.h"
#include <QLabel>
#include <QFrame>
#include <QScrollBar>
#include <QTimer>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include "../constants/Theme.h"

#include "ConversionLabel.h"
#include "MainWindow.h"

static QFont MF(int pt, int w = QFont::Normal) {
    return Theme::monoFont(pt, w);
}

OutputArea::OutputArea(QWidget* parent) : QScrollArea(parent) {
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setStyleSheet(QString(
        "QScrollArea{background:%1;border:none;}"
        "QScrollBar:vertical{background:transparent;width:4px;margin:0;}"
        "QScrollBar::handle:vertical{background:%2;border-radius:2px;min-height:20px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:none;}"
    ).arg(C_OUT, C_DIM));

    m_container = new QWidget;
    m_container->setObjectName("outCont");
    m_container->setStyleSheet(QString("QWidget#outCont{background:%1;}").arg(C_OUT));
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(20, 18, 20, 18);
    m_layout->setSpacing(0);
    m_layout->addStretch();

    setWidget(m_container);
    setWidgetResizable(true);
    addSplash();
}

static QLabel* makeLbl(const QString& html, QFont font, const QString& extraStyle = "") {
    

    auto* l = new QLabel;
    l->setTextFormat(Qt::RichText);
    l->setFont(font);
    l->setText(html);
    l->setStyleSheet("background:transparent;" + extraStyle);
    l->setWordWrap(true);
    return l;
}

// ── ClickableLabel ────────────────────────────────────────────────────────────
// Result label that shows "Copy" on hover and copies plain text on click.
class ClickableLabel : public QLabel {
public:
    ClickableLabel(const QString& html, const QString& plainText,
        const QFont& font, const QString& color,
        const QString& extraStyle = "", QWidget* parent = nullptr, const QString& formula = QString())
        : QLabel(parent)
        , m_html(html)
        , m_plainText(plainText)
        , m_color(color)
        , m_extraStyle(extraStyle)
        , m_formula(formula)
    {
        setTextFormat(Qt::RichText);
        setFont(font);
        setText(html);
        setStyleSheet("background:transparent;" + extraStyle);
        setWordWrap(false);
        setCursor(Qt::PointingHandCursor);
        m_resetTimer = new QTimer(this);
        m_resetTimer->setSingleShot(true);
        connect(m_resetTimer, &QTimer::timeout, this, [this] { resetToNormal(); });
    }

protected:
    void enterEvent(QEnterEvent*) override {
        if (m_copied) return;
        setText(m_html + QString("<span style='color:%1;font-size:10pt;'>"
            " &nbsp;&#10064; Right Click to copy</span>").arg(C_MUTED));
    }
    void leaveEvent(QEvent*) override {
        if (m_copied) return;
        setText(m_html);
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton && !m_formula.isEmpty()) {
            QMessageBox::information(this, "Conversion Formula", m_formula);
        }
        QLabel::mouseDoubleClickEvent(e);
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::RightButton) { QLabel::mousePressEvent(e); return; }
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
    QString  m_formula;
    QString  m_html;
    QString  m_plainText;
    QString  m_color;
    QString  m_extraStyle;
    bool     m_copied = false;
    bool     m_formulaShown = false;
    QTimer* m_resetTimer = nullptr;
};

static ClickableLabel* makeResultLbl(const QString& html, const QString& plainText,
    const QString& color, const QFont& font,
    const QString& extraStyle = "",
    const QString& formula = "")
{

    return new ClickableLabel(html, plainText, font, color, extraStyle, nullptr,formula);
}
void OutputArea::addSplash() {
    int at = m_layout->count() - 1;



    auto* title = makeLbl(
        QString("<span style='color:%1;font-weight:700;'>MATHX Unlimited Calculator</span><span style='color:%2'>--ALPHA</span>").arg(C_ACCENT).arg(C_DRED),
        MF(11, QFont::Bold));
    m_layout->insertWidget(at++, title);
/*
    m_relNotes = new QLabel;
    m_relNotes->setText(QString("<span style='color:%1'>***WARNING***</span><span style='color:%1'>This program is currently in the alpha development phase</span><span style='color:%1'>Expect Bugs and Glitches, More features will be coming soon!</span>").arg(C_ERR).toHtmlEscaped());
    m_relNotes->setFont(MF(9));
    */

  
   /* for (QString line : { "**WARNING**",
        "This program is currently in alpha.",
        "Expect bugs, glitches and runtime errors.",
        "Algebraic simplification and quadratic solving are experimental – some forms may fail.",
        "More features and optimizations coming soon!"
    }) {


        m_layout->insertWidget(m_layout->count() - 1, makeLbl(QString("<span style='color:%1'>%2</span>").arg(C_WARN).arg(line), MF(10)));
    }*/
    m_layout->insertWidget(at++, makeLbl(
        QString("<span style='color:%1;'>"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "</span>").arg(C_DIM), MF(9)));

    for (const QString& t : { "Type any math expression below.",
                               "Click examples in the sidebar to try them.",
                               "Double-click a sidebar example to run it instantly." }) {
        m_layout->insertWidget(at++, makeLbl(
            QString("<span style='color:%1;'>%2</span>").arg(C_MUTED, t), MF(9)));
    }

    m_layout->insertWidget(at++, makeLbl(
        QString("<span style='color:%1;'>"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "</span>").arg(C_DIM), MF(9)));

    struct Ex { QString color, code, rest; };
    for (auto& ex : QList<Ex>{
        { C_ALG,    "88x = 704",            " \xe2\x86\x92 x = 8" },
        { C_PURPLE, "sin(30)",               " \xe2\x86\x92 0.5" },
        { C_INFO,   "tri a=3 b=4 c=5",      " \xe2\x86\x92 full triangle data" },
        { C_WARN,   "100 km to miles",       " \xe2\x86\x92 62.137 miles" },
        { C_ACCENT, "fact(50)",              " \xe2\x86\x92 exact bignum" },
        }) {
        m_layout->insertWidget(at++, makeLbl(
            QString("<span style='color:%1;'>%2</span><span style='color:%3;'>%4</span>")
            .arg(ex.color, ex.code.toHtmlEscaped(), C_MUTED, ex.rest.toHtmlEscaped()),
            MF(9)));
    }

    auto* sp = new QWidget; sp->setFixedHeight(8); sp->setStyleSheet("background:transparent;");
    m_layout->insertWidget(at++, sp);
}

void OutputArea::addInputLine(const QString& expr) {
    auto* sp = new QWidget; sp->setFixedHeight(6); sp->setStyleSheet("background:transparent;");
    m_layout->insertWidget(m_layout->count() - 1, sp);
    m_layout->insertWidget(m_layout->count() - 1, makeLbl(
        QString("<span style='color:%1;font-weight:700;'>&gt;&gt;&nbsp;</span>"
            "<span style='color:%2;'>%3</span>")
        .arg(C_ACCENT, C_TEXT, expr.toHtmlEscaped()),
        MF(9)));
    scrollToBottom();
}

bool OutputArea::progressState() {
    return (m_progressBar);
}

void OutputArea::showProgress(int percent, const QString& label) {
    // Create progress bar if needed
    if (!m_progressBar) {
        m_progressBar = new QProgressBar(m_container);
        m_progressBar->setRange(0, 100);
        m_progressBar->setFormat("%p%");
        m_progressBar->setStyleSheet(
            "QProgressBar {"
            "    background: #2e3530;"
            "    border: none;"
            "    border-radius: 3px;"
            "    text-align: center;"
            "    color: #ddeae0;"
            "}"
            "QProgressBar::chunk {"
            "    background: #00e87a;"
            "    border-radius: 3px;"
            "}"
        );
        m_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_progressBar->setFixedHeight(20);
        // Insert before the stretch (last item)
        m_layout->insertWidget(m_layout->count() - 1, m_progressBar);
    }

    // Create label if needed and update text
    if (!m_progressLabel && !label.isEmpty()) {
        m_progressLabel = new QLabel(m_container);
        m_progressLabel->setFont(Theme::monoFont(8));
        m_progressLabel->setStyleSheet("color:" + Theme::ACCENT + "; background:transparent;");
        // Insert label above the progress bar
        m_layout->insertWidget(m_layout->count() - 2, m_progressLabel);
    }
    if (m_progressLabel) {
        m_progressLabel->setText(label);
    }

    m_progressBar->setValue(percent);
    m_progressBar->show();
    if (m_progressLabel) m_progressLabel->show();
    if (percent >= 99) hideProgress();
}


void OutputArea::hideProgress() {
    if (m_progressBar) {
        m_layout->removeWidget(m_progressBar);
        delete m_progressBar;
        m_progressBar = nullptr;
    }
    if (m_progressLabel) {
        m_layout->removeWidget(m_progressLabel);
        delete m_progressLabel;
        m_progressLabel = nullptr;
    }
}
static QString wrapTextByWidth(const QString& text, const QFont& font, int maxWidth) {
    if (text.isEmpty() || maxWidth <= 0)
        return text;

    QFontMetrics fm(font);
    const int len = text.length();

    // ── Fast path: monospace ──────────────────────────────────────────────────
    // Measure one character. For monospace fonts this gives us the exact number
    // of characters per line via integer division — no search loop needed.
    const int charW = fm.horizontalAdvance(QChar('0'));
    if (charW > 0) {
        const int charsPerLine = maxWidth / charW;
        if (charsPerLine > 0) {
            // Verify with a real measurement before committing to the fast path.
            // horizontalAdvance(QString, int len) measures the FIRST `len` chars
            // of the string — this is the two-argument overload Qt provides.
            const int checkLen = qMin(charsPerLine, len);
            if (fm.horizontalAdvance(text, checkLen) <= maxWidth) {
                QString result;
                result.reserve(len + len / charsPerLine + 1);
                int start = 0;
                while (start < len) {
                    int take = qMin(charsPerLine, len - start);
                    // QStringView::mid avoids allocating a new QString per slice
                    result.append(QStringView(text).mid(start, take));
                    result.append('\n');
                    start += take;
                }
                return result;
            }
        }
    }

    // ── Fallback: binary search (proportional / unusual fonts) ───────────────
    // O(n log n). Each iteration calls horizontalAdvance(QString) with a
    // freshly-allocated mid() substring — unavoidable for proportional fonts
    // since there's no range overload, but log n calls per line keeps it fast.
    QString result;
    result.reserve(len + len / qMax(1, maxWidth / qMax(1, charW)) + 1);
    int start = 0;
    while (start < len) {
        int lo = 1, hi = len - start, best = 1;
        while (lo <= hi) {
            int mid = lo + (hi - lo) / 2;
            if (fm.horizontalAdvance(text.mid(start, mid)) <= maxWidth) {
                best = mid;
                lo = mid + 1;
            }
            else {
                hi = mid - 1;
            }
        }
        result.append(QStringView(text).mid(start, best));
        result.append('\n');
        start += best;
    }
    return result;
}
void OutputArea::addResultLine(const QString& text, const QString& type,
    const QString& copyText,
    const QString& formula,
    const QString& originalExpr) {

    QString result;
    const QString& fullCopy = copyText.isEmpty() ? text : copyText;
    QString color = C_TEXT;
    if (type == "ok")   color = C_ACCENT;
    else if (type == "big")  color = "#00d4ff";
    else if (type == "err")  color = C_ERR;
    else if (type == "geo")  color = C_INFO;
    else if (type == "trig") color = C_PURPLE;
    else if (type == "conv") color = C_WARN;
    else if (type == "alg")  color = C_ALG;

    if (type == "big") {
        int maxWidth = m_container->width() - 80;
        if (maxWidth <= 0) maxWidth = 800;
        QFont font = MF(9);
        QString wrapped = wrapTextByWidth(text, font, maxWidth);
        QStringList lines = wrapped.split('\n', Qt::SkipEmptyParts);
        for (const QString& line : lines) {
            result.append(line);
            QString html = QString("<span style='color:%1;'>%2</span>")
                .arg(color, line.toHtmlEscaped());
            auto* l = makeResultLbl(html, fullCopy, color, font, "padding-left:22px;");
            m_layout->insertWidget(m_layout->count() - 1, l);
            Animations::fadeIn(l);
        }
    }
    else if (type == "conv") {
        QStringList lines = text.split('\n');
        for (const QString& line : lines) {
            result.append(line);
            ConversionLabel* lbl = new ConversionLabel(line, formula, MF(9), color, "padding-left:22px;");
            m_layout->insertWidget(m_layout->count() - 1, lbl);
            Animations::fadeIn(lbl);
        }
    }

    else {
        QStringList lines = text.split('\n');
        for (const QString& line : lines) {
            result.append(line + "\n");
            OutputWidget* widget = new OutputWidget(line, fullCopy, originalExpr, formula, type, color);
            widget->setContentsMargins(0, 0, 0, 0);
            m_layout->insertWidget(m_layout->count() - 1, widget);
            Animations::fadeIn(widget);
        }
    }

    scrollToBottom();
    emit calcFinish(RunState::Idle, result);
}

void OutputArea::addSeparator() {
    auto* sp = new QWidget; sp->setFixedHeight(2); sp->setStyleSheet("background:transparent;");
    m_layout->insertWidget(m_layout->count() - 1, sp);
    auto* sep = new QFrame; sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setStyleSheet(QString("background:%1;border:none;").arg(C_DIM));
    m_layout->insertWidget(m_layout->count() - 1, sep);
    auto* sp2 = new QWidget; sp2->setFixedHeight(2); sp2->setStyleSheet("background:transparent;");
    m_layout->insertWidget(m_layout->count() - 1, sp2);
    scrollToBottom();
}

void OutputArea::clearAll() {
    while (m_layout->count() > 1) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void OutputArea::addGeoCard(GeoCard* card) {
    auto* wrap = new QWidget;
    wrap->setStyleSheet("background: transparent;");
    auto* wl = new QHBoxLayout(wrap);
    wl->setContentsMargins(0, 4, 0, 4);
    wl->setSpacing(0);
    wl->addWidget(card, 1);
    m_layout->insertWidget(m_layout->count() - 1, wrap);
    scrollToBottom();
    connect(card, &GeoCard::showShapeProjection, this, &OutputArea::shapeProjectionRequested);

    emit calcFinish(RunState::Idle, "Geometry Card");
}

void OutputArea::scrollToBottom() {
    QTimer::singleShot(10, this, [this]() {
        verticalScrollBar()->setValue(verticalScrollBar()->maximum());
        });
}

void OutputArea::addPromptRequest(const QString& paramName) {
    
    auto* l = makeLbl(
        QString("<span style='color:%1;'>  %2 </span>"
            "<span style='color:%3;'>=</span>"
            "<span style='color:%4;'> ...</span>")
        .arg(C_TEXT, paramName.toHtmlEscaped(), C_ACCENT, C_MUTED),
        MF(9));
    l->setObjectName("promptRequest");
    m_layout->insertWidget(m_layout->count() - 1, l);
    scrollToBottom();
}


void OutputArea::addPromptAnswer(const QString& paramName, const QString& value) {
    for (int i = m_layout->count() - 2; i >= 0; --i) {
        QLayoutItem* item = m_layout->itemAt(i);
        if (item && item->widget() && item->widget()->objectName() == "promptRequest") {
            item->widget()->deleteLater();
            // takeAt() removes the item AND returns it so we can delete the wrapper
            delete m_layout->takeAt(i);
            break;
        }
    }
    auto* l = makeLbl(
        QString("<span style='color:%1;'>  %2 </span>"
            "<span style='color:%3;'>=</span>"
            "<span style='color:%1;'> %4</span>")
        .arg(C_TEXT, paramName.toHtmlEscaped(), C_ACCENT, value.toHtmlEscaped()),
        MF(9));
    m_layout->insertWidget(m_layout->count() - 1, l);
    scrollToBottom();
}

void OutputArea::captureWidget(QWidget* widget)
{

    m_layout->insertWidget(m_layout->count() - 1, widget);

}

// FIX #9: Replaced the two off-palette hardcoded colors (#b22222, #6495ed)
//         with C_ERR and C_INFO from Theme so they fit the dark green palette.
void OutputArea::addSimplified(const QString& before, const QStringList& after) {
    auto* l = makeLbl(
        QString("<span style='color:%1;'>Simplified: %2</span>"
            "<span style='color:%3;'> to: %4</span>")
        .arg(C_MUTED, before.toHtmlEscaped(),
            C_INFO, after[0].toHtmlEscaped()),
        MF(9));
    m_layout->insertWidget(m_layout->count() - 1, l);
}