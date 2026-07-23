#include "ApproxCard.h"
#include "../constants/Theme.h"

#include <QPainter>
#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>
#include <QResizeEvent>

namespace {

// Indent used for displayed formulas inside the explainer. Plain non-breaking
// spaces rather than a CSS margin: QLabel's rich-text subset renders these
// identically everywhere, and this text has to survive whatever font the user
// picks.
const QString kInd = QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;");

// Result labels carry padding-left:22px. The card has to match it or the
// estimate hangs off the left of everything it belongs to. The MathText line is
// painted in widget coordinates, so it needs the same offset applied by hand -
// the layout margin only moves the child labels.
constexpr int kIndent = 22;

// Math symbols go in as HTML entities, not literal characters. They render the
// same but keep this source pure ASCII, which sidesteps the encoding trouble
// that non-UTF-8 bytes have caused in this codebase before.
//
// Written in the register of "pi is the circumference divided by the diameter":
// explain where the thing comes from, in steps, not just what it is.
QString explanationHtml()
{
    const QString accent = Theme::ACCENT();
    auto head = [&accent](const QString& t) {
        return QStringLiteral("<span style='color:%1'><b>%2</b></span><br>").arg(accent, t);
        };
    auto formula = [&accent](const QString& f) {
        return QStringLiteral("%1<span style='color:%2'>%3</span><br><br>").arg(kInd, accent, f);
        };

    return head(QStringLiteral("What is it?"))
        + QStringLiteral("Stirling's Approximation is a shortcut for working out <i>how big</i> a factorial is, "
            "without multiplying every number in it.<br><br>")

        + head(QStringLiteral("Where does it come from?"))
        + QStringLiteral("<b>n!</b> means 1 &times; 2 &times; 3 &times; &hellip; &times; n. "
            "Products are awkward to reason about, so the first move is to take the "
            "<b>logarithm</b> &mdash; which turns multiplying into adding:<br>")
        + formula(QStringLiteral("ln(n!) = ln 1 + ln 2 + &hellip; + ln n"))

        + QStringLiteral("That sum is the area of a <i>staircase</i> of ln values. As n grows "
            "the staircase hugs the smooth curve y = ln x, so the sum closes in on the "
            "area under that curve:<br>")
        + formula(QStringLiteral("n &middot; ln n &minus; n"))

        + QStringLiteral("Correcting for the sliver the staircase overshoots adds "
            "&frac12; &middot; ln(2&pi;n). Undo the logarithm, and out falls:<br>")
        + formula(QStringLiteral("<b>n! &asymp; &radic;(2&pi;n) &times; (n/e)<sup>n</sup></b>"))

        + head(QStringLiteral("Why bother?"))
        + QStringLiteral("Because it works entirely in logarithms, it hands you the digit "
            "count and the leading digits <i>instantly</i> &mdash; even when actually "
            "multiplying the factorial out would take minutes.<br><br>")

        + head(QStringLiteral("And the second line?"))
        + QStringLiteral("<b>log-gamma</b> is the exact form. The gamma function extends "
            "factorials to every number, not just whole ones, and ln(n!) = lgamma(n+1) "
            "with no approximation at all.<br><br>"
            "So the two lines above are not rival methods &mdash; <b>Stirling is an "
            "approximation <i>of</i> log-gamma</b>. Showing both lets you watch how close "
            "the shortcut lands. For large n they agree to every digit displayed, because "
            "Stirling's leftover error shrinks as 1/(360n<sup>3</sup>) once the 1/(12n) "
            "correction is included.<br><br>")

        + head(QStringLiteral("Who found it?"))
        + QStringLiteral("<b>Abraham de Moivre</b> found the shape of it while working on "
            "probability. What <b>James Stirling</b> added was pinning down the missing "
            "constant &mdash; showing it was &radic;(2&pi;). Most formulas have more than "
            "one parent.");
}

} // namespace

ApproxCard::ApproxCard(const BigNum::FactorialEstimate& est, QWidget* parent)
    : QWidget(parent), m_est(est)
{
    auto* col = new QVBoxLayout(this);
    col->setContentsMargins(kIndent, 2, 0, 4);
    col->setSpacing(2);

    // Row 0 is the MathText formula, painted directly in paintEvent - reserve
    // its height with a spacer so the labels below never overlap it.
    col->addSpacing(int(m_em * 1.6));

    // Both methods, side by side. Stirling is an approximation OF log-gamma, so
    // showing the pair (and how far apart they land) is the actual lesson -
    // silently picking one would hide it.
    m_compare = new QLabel(this);
    m_compare->setFont(Theme::monoFont(8));
    m_compare->setTextFormat(Qt::RichText);
    m_compare->setText(QStringLiteral(
        "<table cellspacing='0' cellpadding='0'>"
        "<tr><td style='color:%1'>&nbsp;&nbsp;Stirling's series&nbsp;&nbsp;&nbsp;</td>"
        "<td style='color:%2'>%3 &times; 10<sup>%4</sup></td></tr>"
        "<tr><td style='color:%1'>&nbsp;&nbsp;log-gamma (exact)&nbsp;&nbsp;&nbsp;</td>"
        "<td style='color:%2'>%5 &times; 10<sup>%6</sup></td></tr>"
        "</table>")
        .arg(Theme::INFO, Theme::ACCENT(),
            m_est.stirling.mantissa, QString::number(m_est.stirling.exponent),
            m_est.gamma.mantissa, QString::number(m_est.gamma.exponent)));
    m_compare->setStyleSheet("background:transparent;");
    col->addWidget(m_compare);

    m_note = new QLabel(this);
    m_note->setFont(Theme::monoFont(8));
    m_note->setTextFormat(Qt::RichText);
    m_note->setStyleSheet("background:transparent;");
    updateNote();
    // One row, two links: '#what' opens the derivation, '#toggle' folds the card.
    connect(m_note, &QLabel::linkActivated, this, [this](const QString& href) {
        if (href == QStringLiteral("#toggle")) setExpanded(!m_expanded);
        else                                   toggleExplanation();
        });
    col->addWidget(m_note);

    // -- The explainer: heading, prose, credit. One block, shown together. -----
    // Titled with the same box-drawing rule the splash and batch headings use,
    // so it reads as a section of the terminal rather than a stray panel.
    const QString rule(10, QChar(0x2500));   // U+2500 BOX DRAWINGS LIGHT HORIZONTAL
    m_heading = new QLabel(this);
    m_heading->setFont(Theme::monoFont(8, QFont::Bold));
    m_heading->setText(rule + QStringLiteral(" Stirling's Approximation ") + rule);
    m_heading->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::ACCENT()));
    m_heading->hide();
    col->addWidget(m_heading);

    m_expl = new QLabel(this);
    m_expl->setFont(Theme::monoFont(8));
    m_expl->setTextFormat(Qt::RichText);
    m_expl->setWordWrap(true);
    m_expl->setText(explanationHtml());
    m_expl->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::INFO));
    m_expl->hide();
    col->addWidget(m_expl);

    m_credit = new QLabel(this);
    m_credit->setFont(Theme::monoFont(7));
    m_credit->setTextFormat(Qt::RichText);
    m_credit->setWordWrap(true);
    m_credit->setText(QStringLiteral(
        "<i>James Stirling (1692-1770) &middot; Methodus Differentialis, 1730</i>"));
    m_credit->setStyleSheet(QString("color:%1; background:transparent;").arg(Theme::INFO));
    m_credit->hide();
    col->addWidget(m_credit);

    relayout();

    // Draw-on paced to match the Geometry Viewer. Two things are copied from it,
    // and the second matters more than the first: the duration, and the fact that
    // progress comes from a WALL CLOCK rather than a fixed step per tick.
    // Stepping per tick makes the speed depend on how often the timer fires.
    m_anim = new QTimer(this);
    connect(m_anim, &QTimer::timeout, this, [this]() {
        m_t = qBound(0.0, m_clock.elapsed() / 1000.0 / MathText::kDrawOnSeconds, 1.0);
        if (m_t >= 1.0) m_anim->stop();
        update();
        });
    m_clock.start();
    m_anim->start(16);   // ~60fps; the clock, not this, sets the speed
}

void ApproxCard::relayout()
{
    // "1000000! ~= 8.2639 x 10" + superscript exponent + " (N digits)"
    MathText::Line line;
    line.run(QStringLiteral("%1! ≈ %2 × 10").arg(m_est.nDisplay, m_est.mantissa));
    line.sup(QString::number(m_est.exponent));
    line.run(QStringLiteral("  (%1 digits)").arg(m_est.digits));
    m_glyphs = line.layout(m_em);
}

QSize ApproxCard::sizeHint() const
{
    return QWidget::sizeHint();
}

void ApproxCard::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    update();
}

void ApproxCard::paintEvent(QPaintEvent*)
{
    if (m_glyphs.isEmpty()) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    MathText::Style st;
    const QColor accent(Theme::ACCENT());
    st.colorFor = [accent](int, QChar) { return accent; };
    st.halo = QColor(0, 0, 0, 0);   // no halo; this sits on the flat terminal bg

    MathText::draw(p, m_glyphs, QPointF(kIndent, m_em * 1.15), st, m_t);
}

bool ApproxCard::explanationVisible() const
{
    return m_expl && m_expl->isVisible();
}

void ApproxCard::setExplanationVisible(bool on)
{
    m_heading->setVisible(on);
    m_expl->setVisible(on);
    m_credit->setVisible(on);
    updateNote();
    updateGeometry();
}

void ApproxCard::toggleExplanation()
{
    setExplanationVisible(!explanationVisible());
}

void ApproxCard::updateNote()
{
    // Collapsed keeps a control row visible so the detail is always reachable;
    // the estimate line itself never hides - it is the point of the card.
    if (!m_expanded) {
        m_note->setText(QStringLiteral(
            "<a href='#toggle' style='color:%1; text-decoration:none'>&#9662; methods</a>")
            .arg(Theme::INFO));
        return;
    }
    const QString what = explanationVisible()
        ? QStringLiteral("Hide") : QStringLiteral("What?");
    m_note->setText(QStringLiteral(
        "&nbsp;&nbsp;<i style='color:%1'>%3</i> "
        "<a href='#what' style='color:%2; text-decoration:none'><i>%4</i></a>"
        "&nbsp;&nbsp;<a href='#toggle' style='color:%1; text-decoration:none'>&#9652; hide</a>")
        .arg(Theme::INFO, Theme::ACCENT(), m_est.agreement, what));
}

void ApproxCard::setExpanded(bool on)
{
    if (m_expanded == on) return;
    m_expanded = on;
    m_compare->setVisible(on);
    if (!on) setExplanationVisible(false);   // collapsing folds the explainer too
    updateNote();
    updateGeometry();
}
