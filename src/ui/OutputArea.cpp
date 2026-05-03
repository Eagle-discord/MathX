#include "OutputArea.h"
#include "../shapes/Shapes2D.h"
#include "../shapes/Shapes3D.h"
#include <QLabel>
#include <QFrame>
#include <QScrollBar>
#include <QTimer>
#include <QFontDatabase>
#include <QFontMetrics>

#include "../theme/Theme.h"
static const QString C_OUT = "#0d100e";   // slightly darker than surface; not in Theme
static const QString C_ACCENT = Theme::ACCENT;
static const QString C_TEXT = Theme::TEXT;
static const QString C_MUTED = Theme::MUTED;
static const QString C_DIM = Theme::DIM;
static const QString C_ERR = Theme::ERROR;
static const QString C_INFO = Theme::INFO;
static const QString C_WARN = Theme::WARN;
static const QString C_PURPLE = Theme::PURPLE;
static const QString C_ALG = Theme::ALG;

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
    l->setWordWrap(false);
    return l;
}

void OutputArea::addSplash() {
    int at = m_layout->count() - 1;

    auto* title = makeLbl(
        QString("<span style='color:%1;font-weight:700;'>MATHX Unlimited Calculator</span>").arg(C_ACCENT),
        MF(11, QFont::Bold));
    m_layout->insertWidget(at++, title);

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

void OutputArea::showProgress(cpp_int percent) {
    if (!m_progressLabel) {
        m_progressLabel = new QLabel(m_container);
        m_progressLabel->setFont(MF(9));
        m_progressLabel->setStyleSheet(
            QString("color:%1;background:transparent;").arg(C_ACCENT));
        int index = m_layout->count();
        if (index > 0) index -= 1;  // insert before the trailing stretch
        m_layout->insertWidget(index, m_progressLabel);
    }
    m_progressLabel->setText(QString("Progress: %1%").arg(percent.str().c_str()));
    m_progressLabel->show();
    m_progressLabel->repaint();
    scrollToBottom();
}

void OutputArea::hideProgress() {
    if (m_progressLabel) {
        m_layout->removeWidget(m_progressLabel);
        m_progressLabel->deleteLater();
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

void OutputArea::addResultLine(const QString& text, const QString& type) {
    QString result;
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

        for (const QString& line : wrapped.split('\n', Qt::SkipEmptyParts)) {
            result.append(line);
            auto* l = makeLbl(
                QString("<span style='color:%1;'>%2</span>").arg(color, line.toHtmlEscaped()),
                font, "padding-left:22px;");
            m_layout->insertWidget(m_layout->count() - 1, l);

        }


    }
    else {
        for (const QString& line : text.split('\n')) {
            result.append(line);
            auto* l = makeLbl(
                QString("<span style='color:%1;'>%2</span>").arg(color, line.toHtmlEscaped()),
                MF(9), "padding-left:22px;");
            m_layout->insertWidget(m_layout->count() - 1, l);

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