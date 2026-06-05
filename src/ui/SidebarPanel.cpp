#include "SidebarPanel.h"
#include <QLabel>
#include <QVBoxLayout>
#include <QFrame>
#include <QMouseEvent>
#include <QEvent>
#include <QFontDatabase>
#include <QFont>

static const QString C_SURFACE = "#0f1511";
static const QString C_TEXT    = "#ddeae0";
static const QString C_MUTED   = "#5a6b5f";
static const QString C_HOVER   = "#1a2820";

// Full-width section bar colors  (background, text)
static QMap<QString,QPair<QString,QString>> barColors() {
    return {
        { "arithmetic",   { "#1a3d2b", "#2d8f5a" } },  // green,  dim
        { "algebra", { "#3a2e0a", "#a88530" } },  // yellow, dim
        { "trigonometry",    { "#2a1a45", "#7a4db5" } },  // purple, dim
        { "geometry",     { "#0d2040", "#2d5fa8" } },  // blue,   dim
        { "conversion",    { "#3a2008", "#b06018" } },  // orange, dim
    };
}

static QString fontFamily() {
    for (const QString& f : {"JetBrains Mono","Consolas","Courier New"})
        if (QFontDatabase::families().contains(f)) return f;
    return "Courier New";
}
static QFont MF(int pt, int w = QFont::Normal) {
    QFont f(fontFamily()); f.setPointSize(pt); f.setWeight((QFont::Weight)w);
    f.setStyleHint(QFont::Monospace); return f;
}

// ── Clickable ref item label ────────────────────────────────────────────────
class RefLabel : public QLabel {
    Q_OBJECT
public:
    RefLabel(const QString& expr, const QString& note, QWidget* parent = nullptr)
        : QLabel(parent), m_expr(expr) {
        QString html = QString("<span style='color:%1;'>%2</span>").arg(C_TEXT, expr.toHtmlEscaped());
        if (!note.isEmpty())
            html += QString("<span style='color:%1;'> // %2</span>").arg(C_MUTED, note.toHtmlEscaped());
        setText(html);
        setTextFormat(Qt::RichText);
        setFont(MF(9));
        setStyleSheet("padding: 3px 14px; background: transparent;");
        setCursor(Qt::PointingHandCursor);
    }
signals:
    void clicked(const QString&);
    void doubleClicked(const QString&);
protected:
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) emit clicked(m_expr);
    }
    void mouseDoubleClickEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) emit doubleClicked(m_expr);
    }
    void enterEvent(QEnterEvent*) override {
        setStyleSheet(QString("padding: 3px 14px; background: %1;").arg(C_HOVER));
    }
    void leaveEvent(QEvent*) override {
        setStyleSheet("padding: 3px 14px; background: transparent;");
    }
private:
    QString m_expr;
};

#include "SidebarPanel.moc"

// ── SidebarPanel ─────────────────────────────────────────────────────────────
SidebarPanel::SidebarPanel(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background: %1;").arg(C_SURFACE));

    m_scroll = new QScrollArea(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { width: 3px; background: transparent; }"
        "QScrollBar::handle:vertical { background: #2e3530; border-radius: 2px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    );

    m_container = new QWidget;
    m_container->setStyleSheet("background: transparent;");
    m_layout = new QVBoxLayout(m_container);
    m_layout->setContentsMargins(0, 8, 0, 8);
    m_layout->setSpacing(0);
    m_layout->addStretch();

    m_scroll->setWidget(m_container);
    m_scroll->setWidgetResizable(true);

    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0,0,0,0);
    outer->addWidget(m_scroll);

    buildRefs();
    renderMode("all");
}

void SidebarPanel::buildRefs() {
    m_refs["all"] = {
        { "arithmetic", {
            { "2 + 3 * (4^2)", "order of ops" },
            { "sqrt(144) + cbrt(27)", "roots" },
            { "fact(10)", "10!" },
            { "nCr(10,3)", "combinations" },
        }},
        { "algebra", {
            { "88x = 704", "linear" },
            { "3x + 7 = 22", "linear" },
            { "x^2 - 5x + 6 = 0", "quadratic" },
        }},
        { "trigonometry", {
            { "sin(30)", "degrees" },
            { "cos(60)", "" },
            { "atan(1)", "= 45\xc2\xb0" },
            { "sinr(pi/6)", "radians" },
        }},
        { "geometry", {
            { "tri(ABC) a=3 b=4 c=5", "triangle" },
            { "circ(O) r=7", "circle" },
            { "sphr(O) r=5", "sphere" },
            { "cube s = 3", "cube"},
        }},
        { "conversion", {
            { "100 km to miles", "" },
            { "72 f to c", "temp" },
            { "5 kg to lbs", "" },
        }},
    };
    m_refs["arithmetic"] = {
        { "arithmetic", {
            { "2 + 3 * (4^2)", "precedence" },
            { "sqrt(144)", "square root" },
            { "cbrt(27)", "cube root" },
            { "abs(-99)", "absolute value" },
            { "log(1000)", "log10 = 3" },
            { "ln(e)", "natural log = 1" },
            { "fact(10)", "10! = 3628800" },
            { "fact(50)", "bignum!" },
            { "nCr(10,3)", "C(10,3)" },
            { "nPr(5,2)", "P(5,2)" },
            { "gcd(48,18)", "HCF" },
            { "lcm(4,6)", "" },
            { "2pi", "\xe2\x89\x88 6.2832" },
        }}
    };
    m_refs["algebra"] = {
        { "algebra", {
            { "88x = 704", "x = 8" },
            { "3x + 7 = 22", "x = 5" },
            { "2x - 9 = x + 3", "x = 12" },
            { "x^2 - 5x + 6 = 0", "x=2, x=3" },
            { "x^2 + 4 = 0", "complex roots" },
            { "x^3 = 27", "Newton method" },
        }}
    };
    m_refs["trigonometry"] = {
        { "trigonometry", {
            { "sin(30)", "= 0.5" },
            { "cos(60)", "= 0.5" },
            { "tan(45)", "= 1" },
            { "asin(0.5)", "= 30\xc2\xb0" },
            { "acos(0.5)", "= 60\xc2\xb0" },
            { "atan(1)", "= 45\xc2\xb0" },
            { "sinr(pi/6)", "radians" },
            { "cosr(pi/3)", "radians" },
            { "sin(30)^2 + cos(30)^2", "= 1 identity" },
        }}
    };
    m_refs["geometry"] = {
        { "geometry", {
            { "tri(ABC) a=3 b=4 c=5", "right triangle" },
            { "tri(ABC) a=5 b=6 c=7", "oblique" },
            { "rect(ABCD) w=8 h=5", "rectangle" },
            { "sqre(ABCD) s=6", "square" },
            { "circ(O) r=7", "circle" },
            { "circ(O) d=14", "from diameter" },
            { "sphr(O) r=5", "sphere" },
            { "cyld(ABC) r=3 h=10", "cylinder" },
            { "cone(O) r=3 h=5", "cone" },
        }}
    };
    m_refs["conversion"] = {
        { "conversion", {
            { "100 km to miles", "" },
            { "5 miles to km", "" },
            { "72 f to c", "temp" },
            { "100 c to f", "" },
            { "0 c to k", "kelvin" },
            { "5 kg to lbs", "" },
            { "180 lbs to kg", "" },
            { "10 ft to m", "" },
            { "1 gallon to l", "liters" },
            { "1 gb to mb", "data" },
            { "30 m/s to kph", "speed" },
        }}
    };
}

void SidebarPanel::clearContent() {
    while (m_layout->count() > 1) {
        QLayoutItem* item = m_layout->takeAt(0);
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }
}

void SidebarPanel::renderMode(const QString& mode) {
    clearContent();
    auto sections = m_refs.value(mode, m_refs["all"]);
    auto bc = barColors();
    int insertAt = 0;

    for (const RefSection& sec : sections) {
        auto [bg, fg] = bc.value(sec.label, { "#00e87a", "#000000" });

        // Full-width flat bar — no border-radius, spans entire width
        QLabel* bar = new QLabel(sec.label.toUpper());
        bar->setFont(MF(8, QFont::Bold));
        bar->setFixedHeight(22);
        bar->setStyleSheet(QString(
            "background: %1; color: %2;"
            "padding: 0px 14px;"
            "letter-spacing: 2px;"
        ).arg(bg, fg));
        m_layout->insertWidget(insertAt++, bar);

        for (const RefItem& item : sec.items) {
            RefLabel* rl = new RefLabel(item.text, item.note);
            connect(rl, &RefLabel::clicked,       this, &SidebarPanel::itemClicked);
            connect(rl, &RefLabel::doubleClicked, this, &SidebarPanel::itemDoubleClicked);
            m_layout->insertWidget(insertAt++, rl);
        }

        // Gap between sections
        auto* gap = new QWidget; gap->setFixedHeight(6);
        gap->setStyleSheet("background: transparent;");
        m_layout->insertWidget(insertAt++, gap);
    }
}
