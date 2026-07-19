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
#include "../constants/Duration.h"
#include <QResizeEvent>
#include "ConversionLabel.h"
#include "MasterLabel.h"
#include "CopyableLabel.h"
#include "../subSystems/WidgetRegistry.h"
#include "MainWindow.h"
#include "ScrubbableExpression.h"
#include "../math/MathEngine.h"     // MathEngine::evalSimple
#include <cmath>
#include <QElapsedTimer>


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

    // Hard-cap the container to the viewport.
    //
    // setWidgetResizable(true) makes the container track the viewport only as a
    // MINIMUM -- it still grows if a child insists on more. Several children do:
    // ClickableLabel (the `big` result label) sets wordWrap(false) with RichText,
    // and QLabel::minimumSizeHint() for a NON-wrapped label is its full natural
    // width. One long line therefore drags the whole container past the right
    // edge, taking the geo cards and their buttons with it -- and because
    // horizontal scrolling is off, that content is simply unreachable.
    //
    // Capping here rather than chasing each child's size policy: it is the one
    // place that is true by definition (the viewport IS the visible width) and
    // it cannot be re-broken by adding a new result widget later.
    m_container->setMaximumWidth(viewport()->width());

    addSplash();
}

void OutputArea::resizeEvent(QResizeEvent* e) {
    QScrollArea::resizeEvent(e);
    // Keep the cap in step with the viewport. Cheap: one setMaximumWidth on one
    // widget, not a re-wrap of every line (which is the O(n^2) trap -- see
    // OutputWidget::sizeHint).
    if (m_container) m_container->setMaximumWidth(viewport()->width());
}

// -- makeLbl ---------------------------------------------------------------
// Returns a MasterLabel (not a plain QLabel) so every simple terminal line
// participates in the widget inspector the same way as OutputWidget/
// ConversionLabel/ClickableLabel do.
static MasterLabel* makeLbl(const QString& html, QFont font, const QString& extraStyle = "") {
    auto* l = new MasterLabel;
    l->setTextFormat(Qt::RichText);
    l->setFont(font);
    l->setText(html);
    l->setStyleSheet("background:transparent;" + extraStyle);
    l->setWordWrap(true);
    return l;
}

// -- ClickableLabel ------------------------------------------------------------
// Result label that shows "Copy" on hover and copies plain text on right
// click. Left double-click shows the conversion formula (if any) in a popup.
// A thin CopyableLabel subclass — all copy/hover/timer logic lives there,
// and the widget-inspector hook lives further up in MasterLabel.
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
        //      container wider than the viewport, dragging the geo cards and
        //      their buttons off the right edge -- unreachable, since horizontal
        //      scrolling is off.
        //   2) The <br>s are computed from the viewport width AT INSERT TIME and
        //      never recomputed. Shrink the window and they are stale; wrap is
        //      the safety net that keeps stale breaks from overflowing.
        // The <br>s still do the real work; this just stops the label lying about
        // how narrow it can be.
        //
        // NOTE: there is a SECOND ClickableLabel in ui/ClickableLabel.h. This
        // file-local one shadows it and is what makeResultLbl actually builds.
        setWordWrap(true);

        setupCopyable(plainText, color, formula,
            DetailTrigger::DoubleClickPopup, HoverHint::Always);

    }

    // Live-update hook (see CopyableLabel::setNormalText). m_html is what
    // normalText() returns; refreshDisplay() rebuilds the label from it on every
    // hover and copy-flash reset, so a bare setText() would be silently reverted.
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

void OutputArea::addSplash() {
    int at = m_layout->count() - 1;

    auto* title = makeLbl(
        QString("<span style='color:%1;font-weight:700;'>MATHX Unlimited Calculator</span><span style='color:%2'>--ALPHA</span>").arg(C_ACCENT).arg(C_DRED),
        MF(11, QFont::Bold));
    trackLabel(title, WidgetRole::FontSplash);
    m_layout->insertWidget(at++, title);

    m_layout->insertWidget(at++, makeLbl(
        QString("<span style='color:%1;'>"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "</span>").arg(C_DIM), MF(9)));
    // (dividers are decorative box-drawing rules, not registered — nothing
    // meaningful to inspect or resize independently)

    for (const QString& t : { "Type any math expression below.",
                               "Click examples in the sidebar to try them.",
                               "Double-click a sidebar example to run it instantly." }) {
        auto* l = makeLbl(
            QString("<span style='color:%1;'>%2</span>").arg(C_MUTED, t), MF(9));
        trackLabel(l, WidgetRole::FontSplash);
        m_layout->insertWidget(at++, l);
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
        auto* l = makeLbl(
            QString("<span style='color:%1;'>%2</span><span style='color:%3;'>%4</span>")
            .arg(ex.color, ex.code.toHtmlEscaped(), C_MUTED, ex.rest.toHtmlEscaped()),
            MF(9));
        trackLabel(l, WidgetRole::FontSplash);
        m_layout->insertWidget(at++, l);
    }

    auto* sp = new QWidget; sp->setFixedHeight(8); sp->setStyleSheet("background:transparent;");
    m_layout->insertWidget(at++, sp);
}

void OutputArea::addBatchHeading(const QString& text) {
    auto* lbl = new MasterLabel(m_container);
    lbl->setTextFormat(Qt::RichText);
    lbl->setFont(MF(9));
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setStyleSheet("background:transparent;");
    lbl->setText(QString(
        "<span style='color:%1; letter-spacing:2px;'>%2</span>"
    ).arg(Theme::MUTED, text.toHtmlEscaped()));
    trackLabel(lbl, WidgetRole::FontSeparators);
    m_layout->insertWidget(m_layout->count() - 1, lbl);
    scrollToBottom();
}

ScrubbableExpression* OutputArea::addInputLine(const QString& expr) {
    auto* sp = new QWidget;
    sp->setFixedHeight(6);
    sp->setStyleSheet("background:transparent;");
    m_layout->insertWidget(m_layout->count() - 1, sp);

    auto* line = new ScrubbableExpression(expr);
    line->setFont(MF(9));
    line->setPrefix(QStringLiteral(">> "));
    line->setPrefixColor(QColor(C_ACCENT));
    line->setStyleSheet(QString("background:transparent; color:%1;").arg(C_TEXT));

    trackLabel(line, WidgetRole::FontInput);
    m_layout->insertWidget(m_layout->count() - 1, line);

    // Claimed by the next addResultLine(). If none arrives (batch heading,
    // splash, prompt), it simply stays unbound and nothing happens.
    m_pendingInput = line;

    scrollToBottom();
    return line;
}


bool OutputArea::progressState() {
    return (m_progressBar);
}

void OutputArea::showProgress(int percent, const QString& label) {
    if (!m_progressBar) {
        m_progressBar = new QProgressBar(m_container);
        m_progressBar->setRange(0, 100);
        // Slim, continuous bar: no inline text (the percent rides in the label
        // above), a thin rounded track, and the live accent colour so it follows
        // the theme instead of the old hardcoded green.
        m_progressBar->setTextVisible(false);
        m_progressBar->setStyleSheet(QString(
            "QProgressBar{background:%1;border:none;border-radius:2px;}"
            "QProgressBar::chunk{background:%2;border-radius:2px;}"
        ).arg(C_DIM, Theme::ACCENT()));
        m_progressBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_progressBar->setFixedHeight(4);
        m_layout->insertWidget(m_layout->count() - 1, m_progressBar);
    }

    if (!m_progressLabel) {
        m_progressLabel = new QLabel(m_container);
        m_progressLabel->setFont(Theme::monoFont(8));
        m_progressLabel->setStyleSheet("color:" + Theme::ACCENT() + "; background:transparent;");
        m_layout->insertWidget(m_layout->count() - 2, m_progressLabel);
    }
    if (m_progressLabel) {
        // "Calculating: 42%" -- percent lives in the label since the slim bar has
        // no room for text.
        const QString pct = QString::number(percent) + "%";
        m_progressLabel->setText(label.isEmpty() ? pct : label + ": " + pct);
    }

    m_progressBar->setValue(percent);
    m_progressBar->show();
    if (m_progressLabel) m_progressLabel->show();
    // Deliberately NOT hiding at >= 99. The old code did, which meant a job with
    // a post-calculation phase (Formatting, emitted at 100) hid the bar before
    // that phase could ever be shown -- and any calculation flickered out at 99%
    // while still working. hideProgress() is called from onWorkerFinish instead,
    // when the job is actually done.
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

    const int charW = fm.horizontalAdvance(QChar('0'));
    if (charW > 0) {
        const int charsPerLine = maxWidth / charW;
        if (charsPerLine > 0) {
            const int checkLen = qMin(charsPerLine, len);
            if (fm.horizontalAdvance(text, checkLen) <= maxWidth) {
                QString result;
                result.reserve(len + len / charsPerLine + 1);
                int start = 0;
                while (start < len) {
                    int take = qMin(charsPerLine, len - start);
                    result.append(QStringView(text).mid(start, take));
                    result.append('\n');
                    start += take;
                }
                return result;
            }
        }
    }

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
// -- buildResultHtml ---------------------------------------------------------
// Builds result html exactly the way addResultLine's "big"/else branches do:
// wrap to the container width, one <span> per line, joined with <br>.
//
// The live path MUST use this same builder. A naive single-span rebuild renders
// a long result as one clipped line, because ClickableLabel sets wordWrap(false)
// and the <br>s are the only thing breaking a 65-digit number across rows.
static QString buildResultHtml(const QString& text, const QString& color,
    int maxWidth, const QFont& font,
    double calcTime, bool withTiming)
{
    QString html;
    // Preserve author line breaks first (conv/alg/equality results are
    // multi-line), THEN width-wrap each line. wrapTextByWidth treats its input
    // as a single run, so feeding it "\n"-joined text would mash the lines onto
    // one row -- which is exactly what the live-scrub preview used to do.
    const QStringList sourceLines = text.split('\n');
    for (const QString& srcLine : sourceLines) {
        const QString wrapped = wrapTextByWidth(srcLine, font, maxWidth);
        const QStringList lines = wrapped.split('\n', Qt::SkipEmptyParts);
        if (lines.isEmpty()) {
            html.append("<br>");   // keep a deliberate blank line
            continue;
        }
        for (const QString& line : lines)
            html.append(QString("<span style='color:%1;'>%2</span><br>")
                .arg(color, line.toHtmlEscaped()));
    }

    if (withTiming)
        html.append(QString("<span style='color:%1;'>%2</span>")
            .arg(Theme::MUTED, Duration::calculatedIn(calcTime)));
    return html;
}
// Display form of a long result: first `limit` chars plus a "N more" banner.
// The FULL string is what goes to copyText -- never truncate that.
//
// This used to live in MainWindow::onWorkerFinish, which meant the live-scrub
// and retarget paths bypassed it entirely and dumped 1786 digits into a label.
static QString displayTextFor(const QString& s, int limit = 500) {
    const int full = s.length();
    if (full <= limit) return s;
    QString d = s.left(limit);
    d.append(QString("\n.... %1 more characters (%2 total)")
        .arg(full - limit).arg(full));
    return d;
}
void OutputArea::bindLivePair(ScrubbableExpression* expr, CopyableLabel* result,
    const ResultType& type, const QString& color)
{
    if (!expr || !result) {
        qWarning() << "[scrub] bindLivePair skipped: expr=" << (void*)expr
            << "result=" << (void*)result << "type=" << ResultTypes::toString(type)
            << "-- this input line will drag but never recompute";
        return;
    }

    // Every result type recomputes live during a scrub EXCEPT geo, whose
    // "result" is an interactive card rather than a value string. Pure
    // arithmetic (ok/big/trig) takes a fast exact path via bigEval; everything
    // else (conv/alg/func/equality-checks/...) falls back to the full engine,
    // which is still sub-millisecond for the expressions people actually scrub.
    const bool liveOk = (type != ResultType::geo);

    QPointer<ScrubbableExpression> safeExpr(expr);
    QPointer<CopyableLabel>        safeResult(result);

    connect(expr, &ScrubbableExpression::expressionEdited, this,
        [this, safeExpr, safeResult, liveOk, color]
        (const QString& newExpr, bool live) {

            if (safeExpr.isNull() || safeResult.isNull()) return;

            if (!live) {
                emit expressionRecomputeRequested(newExpr, safeResult.data());
                return;
            }

            if (!liveOk) return;

            // -- live evaluation, timed --------------------------------------
            // Note this measures ONLY the evaluation. The committed path's
            // number also includes the worker queue and thread hop, so a live
            // scrub legitimately reads ~0.01 ms against a committed ~50 ms.
            // That difference is the whole point of the two-path design.
            QElapsedTimer timer;
            timer.start();

            QString shown;
            bool bok = false;
            const QString bigResult = MathEngine::bigEval(newExpr, bok);

            if (bok) {
                // Fast exact path: pure arithmetic (incl. large integers).
                shown = bigResult;
            }
            else {
                // Full engine for conv / alg / func / equality-checks / etc.
                // Also recovers the exact bignum path (e.g. factorials) that
                // bigEval declined, so results never downgrade to a rounded
                // double the way the old evalSimple fallback did.
                CalcResult r;
                try { r = MathEngine::evaluate(newExpr); }
                catch (...) { return; }   // transient invalid state mid-drag

                // Nothing worth previewing: unhandled, a geometry card, or an
                // error we don't want flashing on every pixel. Leave the last
                // good result on screen; release runs the full committed path.
                if (r.type == ResultType::none || r.type == ResultType::geo ||
                    r.type == ResultType::err || r.result.isEmpty())
                    return;

                shown = r.result;
            }

            const double liveMs = timer.nsecsElapsed() / 1.0e6;

            // viewport(), not m_container -- see the note in addResultLine.
            int maxWidth = viewport()->width() - 80;
            if (maxWidth <= 0) maxWidth = 800;

            // MUST go through setNormalText(), not setText(). Every
            // CopyableLabel caches its display string in a private member that
            // normalText() returns, and refreshDisplay() rebuilds the label
            // from it on every hover and copy-flash reset -- a bare setText()
            // is silently reverted the next time the mouse crosses the widget.
            safeResult->setNormalText(
                buildResultHtml(displayTextFor(shown), color, maxWidth, MF(9),
                    liveMs, /*withTiming=*/true));
            safeResult->setCopyText(shown);      // FULL value, untruncated
        });
}
QString OutputArea::colorForType(ResultType type) {
    if (type == ResultType::ok)   return C_ACCENT;
    if (type == ResultType::big)  return "#00d4ff";
    if (type == ResultType::err)  return C_ERR;
    if (type == ResultType::geo)  return C_INFO;
    if (type == ResultType::trig) return C_PURPLE;
    if (type == ResultType::conv) return C_WARN;
    if (type == ResultType::alg)  return C_ALG;
    if (type == ResultType::func) return "#4fd6c0";   // teal — user-function result
    return C_TEXT;
}

void OutputArea::updateResultLine(CopyableLabel* target, const QString& text,
    ResultType type, double calcTime)
{
    if (!target) return;

    const QString color = colorForType(type);

    // viewport(), not m_container -- see the note in addResultLine.
    int maxWidth = viewport()->width() - 80;
    if (maxWidth <= 0) maxWidth = 800;

    QString html;
    if (type == ResultType::func) {
        // The func branch prefixes a glyph; keep it on a rewrite.
        html = QString("<span style='color:%1;font-weight:600;'>\xC6\x92 </span>")
            .arg(color);
        html += buildResultHtml(text, color, maxWidth, MF(9), calcTime, true);
    }
    else {
        html = buildResultHtml(text, color, maxWidth, MF(9), calcTime, true);

    }

    target->setColor(color);        // type may have changed (e.g. big -> err)
    target->setNormalText(html);
    target->setCopyText(text);      // clean value, not the html

    if (auto* ow = qobject_cast<OutputWidget*>(target))
        ow->setType(type);          // OutputWidget-only Q_PROPERTY
}

void OutputArea::addResultLine(const QString& text, ResultType type,
    const QString& copyText,
    const QString& formula,
    const QString& originalExpr,
    double calcTime) {

    const QString& fullCopy = copyText.isEmpty() ? text : copyText;
    const QString color = colorForType(type);

    QFont font = MF(9);
    // viewport(), not m_container. The container is the scroll area's child
    // widget: with setWidgetResizable(true) it TRACKS the viewport, but it is
    // still free to grow if a child demands more (which is exactly what a
    // too-large minimumSizeHint on a result label used to do -- see
    // OutputWidget::sizeHint). When that happened the wrap width grew with it,
    // so the text never wrapped and ran off the right edge. The viewport is the
    // visible width by definition and cannot be pushed wider by content.
    int maxWidth = viewport()->width() - 80;
    if (maxWidth <= 0) maxWidth = 800;

    auto wrapToHtml = [&](const QString& input) -> QString {
        QString wrapped = wrapTextByWidth(input, font, maxWidth);
        return wrapped.toHtmlEscaped();
        };

    if (type == ResultType::big) {
        const QString html = buildResultHtml(displayTextFor(text), color,
            maxWidth, font, calcTime, true);
        auto* l = makeResultLbl(html, fullCopy, color, font, "padding-left:22px;");
        trackLabel(l, WidgetRole::FontResults);
        bindLivePair(m_pendingInput, l, type, color);

        m_layout->insertWidget(m_layout->count() - 1, l);
        Animations::fadeIn(l);
    }
    else if (type == ResultType::conv) {
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        QString html;
        for (const QString& line : lines) {
            html.append(QString("<span style='color:%1;'>%2</span>")
                .arg(color, line).append('\n'));
        }
        html = html.replace("\n", "<br>");
        html.append(QString("<span style='color:%1;'>%2</span>").arg(Theme::MUTED, Duration::calculatedIn(calcTime)));
        QString plain = html;
        plain.replace("<br>", "\n");
        // NOTE: no toHtmlEscaped() — this string goes to the clipboard.
        // Escaping it there produced "&amp;" in pasted output.
        auto* lbl = new ConversionLabel(html, plain, formula, font, color,
            "padding-left:22px;");
        trackLabel(lbl, WidgetRole::FontResults);
        bindLivePair(m_pendingInput, lbl, type, color);
        m_layout->insertWidget(m_layout->count() - 1, lbl);
        Animations::fadeIn(lbl);
    }
    else if (type == ResultType::func) {
        // "ƒ <value>", colour-coded, with the same left-click expand/collapse as
        // plain arithmetic when a full-digit form is available.
        auto buildFuncHtml = [&](const QString& valueText) {
            return QString("<span style='color:%1;font-weight:600;'>\xC6\x92 </span>")
                .arg(color)
                + buildResultHtml(valueText, color, maxWidth, font, calcTime, true);
            };

        const QString collapsedHtml = buildFuncHtml(text);
        auto* widget = new OutputWidget(collapsedHtml, fullCopy, originalExpr,
            formula, type, color);
        widget->setContentsMargins(0, 0, 0, 0);

        if (!formula.isEmpty()) {
            const QString expandedHtml = buildFuncHtml(displayTextFor(formula));
            widget->setExpandable(collapsedHtml, expandedHtml, fullCopy, formula);
        }

        trackLabel(widget, WidgetRole::FontResults);
        bindLivePair(m_pendingInput, widget, type, color);
        m_layout->insertWidget(m_layout->count() - 1, widget);
        Animations::fadeIn(widget);
    }
    else {
        const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        QString combined;
        for (int i = 0; i < lines.size(); ++i) {
            combined += wrapToHtml(lines[i]);
            if (i < lines.size() - 1) combined += '\n';
        }
        combined = combined.replace("\n", "<br>");
        combined.append(QString("<span style='color:%1;'>%2</span>").arg(Theme::MUTED, Duration::calculatedIn(calcTime)));
        QString fullHtml = QString("<span style='color:%1;'>%2</span>")
            .arg(color, combined);
        auto* widget = new OutputWidget(fullHtml, fullCopy, originalExpr,
            formula, type, color);
        widget->setContentsMargins(0, 0, 0, 0);

        // Numeric results carry their full-digit form in `formula` when it
        // differs from the collapsed (scientific) value. Wire left-click
        // expand/collapse: collapsed shows the ~15-sig-fig value, expanded shows
        // every digit (capped for display; the full value still goes to copy).
        if (type == ResultType::ok && !formula.isEmpty()) {
            const QString expandedHtml = buildResultHtml(
                displayTextFor(formula), color, maxWidth, font, calcTime, true);
            widget->setExpandable(fullHtml, expandedHtml, fullCopy, formula);
        }

        trackLabel(widget, WidgetRole::FontResults);
        bindLivePair(m_pendingInput, widget, type, color);
        m_layout->insertWidget(m_layout->count() - 1, widget);
        Animations::fadeIn(widget);
    }

    // Consumed (or discarded, for big/conv/geo which aren't OutputWidgets).
    // MUST be cleared on EVERY path, or the next result binds to a stale
    // input line and you get a cross-wired pair.
    m_pendingInput = nullptr;

    scrollToBottom();

}
void OutputArea::addSeparator() {
    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setFixedHeight(1);
    sep->setContentsMargins(0, 2, 0, 2);
    sep->setStyleSheet(QString("background:%1;border:none;").arg(C_DIM));
    m_layout->insertWidget(m_layout->count() - 1, sep);

    scrollToBottom();
}

void OutputArea::clearAll() {
    while (m_layout->count() > 1) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void OutputArea::addGeoCard(GeoCard* card, const double calcTime) {
    auto* wrap = new QWidget;
    wrap->setStyleSheet("background: transparent;");
    auto* wl = new QHBoxLayout(wrap);
    wl->setContentsMargins(0, 4, 0, 4);
    wl->setSpacing(0);
    wl->addWidget(card, 1);

    auto* time = new MasterLabel(
        QString("<span style='color:%1;'>%2</span>")
        .arg(C_MUTED, Duration::calculatedIn(calcTime)));
    time->setFont(MF(9));
    time->setContentsMargins(22, 0, 0, 4);
    trackLabel(time, WidgetRole::FontResults);

    m_layout->insertWidget(m_layout->count() - 1, wrap);
    m_layout->insertWidget(m_layout->count() - 1, time);

    scrollToBottom();

    connect(card, &GeoCard::showShapeProjection, this, &OutputArea::shapeProjectionRequested);


}

void OutputArea::scrollToBottom() {
    if (m_scrollPending) return;          // already queued — coalesce
    m_scrollPending = true;
    QTimer::singleShot(10, this, [this]() {
        m_scrollPending = false;
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
    trackLabel(l, WidgetRole::FontInput);
    m_layout->insertWidget(m_layout->count() - 1, l);
    scrollToBottom();
}

void OutputArea::addPromptAnswer(const QString& paramName, const QString& value) {
    for (int i = m_layout->count() - 2; i >= 0; --i) {
        QLayoutItem* item = m_layout->itemAt(i);
        if (item && item->widget() && item->widget()->objectName() == "promptRequest") {
            item->widget()->deleteLater();
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
    trackLabel(l, WidgetRole::FontInput);
    m_layout->insertWidget(m_layout->count() - 1, l);
    scrollToBottom();
}

void OutputArea::captureWidget(QWidget* widget)
{
    m_layout->insertWidget(m_layout->count() - 1, widget);
}

void OutputArea::addSimplified(const QString& before, const QStringList& after) {
    auto* l = makeLbl(
        QString("<span style='color:%1;'>Simplified: %2</span>"
            "<span style='color:%3;'> to: %4</span>")
        .arg(C_MUTED, before.toHtmlEscaped(),
            C_INFO, after[0].toHtmlEscaped()),
        MF(9));
    trackLabel(l, WidgetRole::FontResults);
    m_layout->insertWidget(m_layout->count() - 1, l);
}