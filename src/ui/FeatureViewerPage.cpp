#include "FeatureViewerPage.h"
#include "../constants/Theme.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QMouseEvent>
#include <QEvent>
#include <vector>

namespace {

struct Feature { QString example; QString desc; };
struct Category { QString title; QString bg; QString fg; std::vector<Feature> items; };

// The catalogue. Kept in code (not read from a file) so an example is always a
// live, runnable expression the engine actually understands.
static std::vector<Category> catalogue() {
    return {
        { "Arithmetic & Big Numbers", "#1a3d2b", "#2d8f5a", {
            { "2 + 3 * (4 ^ 2)",  "operator precedence" },
            { "0.1 + 0.2",        "exact decimals (= 0.3)" },
            { "2 ^ 1000",         "arbitrary-precision power" },
            { "fact(50)",         "exact big factorial" },
            { "1e6 * 2.5",        "scientific-notation input" },
            { "50% * 200",        "percent" },
        }},
        { "Built-in Functions", "#0d2040", "#2d5fa8", {
            { "sqrt(144) + cbrt(27)", "roots" },
            { "log(1000)",            "log base 10" },
            { "ln(e)",                "natural log" },
            { "gcd(48, 36)",          "greatest common divisor" },
            { "nCr(10, 3)",           "combinations" },
            { "hypot(3, 4)",          "hypotenuse" },
        }},
        { "Algebra", "#3a2e0a", "#a88530", {
            { "88x = 704",            "solve linear" },
            { "x^2 - 5x + 6 = 0",     "solve quadratic" },
            { "2x + 3x + (4x - x)",   "simplify" },
            { "factor(x^2 - 9)",      "factorise" },
        }},
        { "Trigonometry", "#2a1a45", "#7a4db5", {
            { "sin(30)",       "degrees by default" },
            { "atan(1)",       "= 45 degrees" },
            { "sinr(pi / 6)",  "radian variant" },
            { "sinh(1)",       "hyperbolic" },
        }},
        { "Geometry", "#0d2040", "#2d5fa8", {
            { "circle r = 7",       "interactive card" },
            { "tri a=3 b=4 c=5",    "triangle" },
            { "sphere r = 5",       "3D viewer" },
            { "cube s = 3",         "" },
        }},
        { "Unit Conversion", "#3a2008", "#b06018", {
            { "100 km to miles",  "" },
            { "72 f to c",        "temperature" },
            { "mach 2 to km/h",   "natural input" },
            { "30 m/s to kph",    "compound units" },
        }},
        { "Natural Language", "#2a1a45", "#7a4db5", {
            { "half of 80",          "" },
            { "3 times 7",           "" },
            { "5 percent of 200",    "" },
            { "what is 15 squared",  "" },
        }},
    };
}

// A clickable example row: "example  // description", with a hover highlight.
class ExampleRow : public QLabel {
    Q_OBJECT
public:
    ExampleRow(const Feature& f, QWidget* parent = nullptr)
        : QLabel(parent), m_expr(f.example)
    {
        QString html = QString("<span style='color:%1;'>%2</span>")
            .arg(Theme::TEXT, f.example.toHtmlEscaped());
        if (!f.desc.isEmpty())
            html += QString("<span style='color:%1;'>&nbsp;&nbsp;// %2</span>")
                .arg(Theme::MUTED, f.desc.toHtmlEscaped());
        setText(html);
        setTextFormat(Qt::RichText);
        setFont(Theme::monoFont(10));
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("Click to run"));
        applyBg(QStringLiteral("transparent"));
    }

signals:
    void clicked(const QString& expr);

protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) emit clicked(m_expr);
    }
    void enterEvent(QEnterEvent*) override { applyBg(Theme::HOVER); }
    void leaveEvent(QEvent*) override { applyBg(QStringLiteral("transparent")); }

private:
    void applyBg(const QString& bg) {
        setStyleSheet(QString("padding: 5px 20px; background: %1;").arg(bg));
    }
    QString m_expr;
};

} // namespace

FeatureViewerPage::FeatureViewerPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;").arg(Theme::BG));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // -- Top bar: title + back button ------------------------------------------
    auto* topBar = new QWidget;
    topBar->setFixedHeight(48);
    topBar->setStyleSheet(QString(
        "background:%1;border-bottom:1px solid %2;").arg(Theme::CARD, Theme::BORDER));
    auto* topL = new QHBoxLayout(topBar);
    topL->setContentsMargins(20, 0, 12, 0);

    auto* title = new QLabel(QStringLiteral("FEATURES"));
    title->setFont(Theme::monoFont(11, QFont::Bold));
    title->setStyleSheet(QString("color:%1;letter-spacing:3px;background:transparent;")
        .arg(Theme::ACCENT()));

    auto* backBtn = new QPushButton(QStringLiteral("‹ Back"));
    backBtn->setFont(Theme::monoFont(9));
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFixedHeight(28);
    backBtn->setStyleSheet(QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;"
        "border-radius:6px;padding:2px 12px;}"
        "QPushButton:hover{border-color:%3;color:%3;}"
    ).arg(Theme::BORDER, Theme::MUTED, Theme::ACCENT()));
    connect(backBtn, &QPushButton::clicked, this, &FeatureViewerPage::backClicked);

    topL->addWidget(title, 0, Qt::AlignVCenter);
    topL->addStretch(1);
    topL->addWidget(backBtn, 0, Qt::AlignVCenter);
    root->addWidget(topBar);

    // -- Scrollable catalogue --------------------------------------------------
    auto* scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollBar:vertical{width:4px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#2e3530;border-radius:2px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
    );

    auto* container = new QWidget;
    container->setStyleSheet("background:transparent;");
    auto* col = new QVBoxLayout(container);
    col->setContentsMargins(0, 8, 0, 24);
    col->setSpacing(0);

    auto* intro = new QLabel(QStringLiteral(
        "Everything the calculator can do. Click any example to run it."));
    intro->setFont(Theme::monoFont(9));
    intro->setStyleSheet(QString("color:%1;padding:8px 20px 14px;background:transparent;")
        .arg(Theme::MUTED));
    col->addWidget(intro);

    for (const Category& cat : catalogue()) {
        auto* bar = new QLabel(cat.title.toUpper());
        bar->setFont(Theme::monoFont(9, QFont::Bold));
        bar->setFixedHeight(26);
        bar->setStyleSheet(QString(
            "background:%1;color:%2;padding:0 20px;letter-spacing:2px;")
            .arg(cat.bg, cat.fg));
        col->addWidget(bar);

        for (const Feature& f : cat.items) {
            auto* row = new ExampleRow(f);
            connect(row, &ExampleRow::clicked, this, &FeatureViewerPage::runExample);
            col->addWidget(row);
        }

        auto* gap = new QWidget;
        gap->setFixedHeight(10);
        gap->setStyleSheet("background:transparent;");
        col->addWidget(gap);
    }

    col->addStretch(1);
    scroll->setWidget(container);
    root->addWidget(scroll, 1);
}

#include "FeatureViewerPage.moc"
