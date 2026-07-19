#include "WidgetEditorPage.h"
#include "../constants/Theme.h"
#include "../math/BigNum.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QColorDialog>
#include <QMetaProperty>
#include <QMetaEnum>
#include <QScrollArea>
#include <QFrame>
#include <QMap>
#include <QVector>
#include <QPair>
#include <QComboBox>
#include <QFontComboBox>
#include <limits>
#include <cmath>
#include <algorithm>
#include <QRegularExpression>

static QFont MF(int pt, int w = QFont::Normal) { return Theme::monoFont(pt, w); }

// -- BigNum-backed numeric expression evaluation --------------------------
// Int/Double properties are edited as plain text rather than a bounded
// QSpinBox/QDoubleSpinBox, so the field can accept full expressions —
// "2^100", "sqrt(2)", "5!"-style factorials via bigFactorial, pi, e, etc. —
// evaluated at arbitrary precision via BigNum::bigEval. The *result* still
// has to be squeezed into whatever native type the underlying Qt property
// actually is (a plain int or double member), so precision beyond that is
// necessarily lost on assignment — the win is that the expression itself
// can involve numbers far larger than int/double could hold mid-calculation.
static double evaluateBigExpr(const QString& text, bool& ok) {
    ok = false;
    const QString result = BigNum::bigEval(text, ok);
    if (!ok) return 0.0;

    bool numOk = false;
    const double val = result.toDouble(&numOk);
    ok = numOk;
    return val;
}

// Small "ƒx" hint placed next to expression-capable fields so it's clear at
// a glance they take formulas, not just a literal number — same spirit as
// the dropdown chevron hint used elsewhere in this file.
static QLabel* buildExprHintLabel() {
    auto* hint = new QLabel("\u0192x");
    hint->setFont(MF(8, QFont::Bold));
    hint->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::MUTED));
    hint->setToolTip("Accepts expressions: + - * / ^, sqrt(), pow(), abs(), "
        "log(), ln(), exp(), sin(), cos(), floor(), ceil(), pi, e");
    return hint;
}

// -- Section categorization ---------------------------------------------
// Properties are bucketed into a handful of human-friendly sections based
// on simple keyword matches against the property name. This is intentionally
// heuristic rather than an explicit per-class map: it keeps new widget
// classes "just working" without needing to register every property they
// expose, at the cost of the odd property landing in a less-than-perfect
// bucket. "General" is the catch-all for anything that doesn't match.
//
// SECTION_ORDER controls both which sections exist and the order they are
// displayed in; sections with no matching properties for a given widget are
// simply skipped.
static const QVector<QString> SECTION_ORDER = {
    "Text", "Appearance",  "General", "Geometry", "Behavior"
};

// -- Display-name aliasing ------------------------------------------------
// Qt's raw property names are sometimes ambiguous or misleading once shown
// next to their siblings — e.g. QLabel/QTextEdit's "text" property actually
// holds rich/HTML content, which is easy to confuse with "plainText" (the
// stripped-down sibling some widgets also expose). This table gives the
// confusing ones an explicit, clearer label. Anything not listed here falls
// back to a humanized version of the camelCase identifier (see
// humanizeIdentifier below) rather than showing the raw name verbatim.
static QString aliasForProperty(const QString& name) {
    static const QMap<QString, QString> aliases = {
        { "text",         "Text (HTML)" },
        { "plainText",    "Text (Plain)" },
        { "html",         "HTML Source" },
        { "styleSheet",   "Style Sheet (QSS)" },
        { "echoMode",     "Echo Mode (Password Masking)" },
        { "toolTip",      "Tooltip" },
        { "whatsThis",    "\"What's This\" Help" },
        { "windowTitle",  "Window Title" },
        { "cursor",       "Cursor Shape" },
    };
    return aliases.value(name);
}

// Fallback for anything not covered by aliasForProperty: turns a camelCase
// identifier like "minimumWidth" into "Minimum Width" so the list never
// shows a raw, unspaced Qt property name.
static QString humanizeIdentifier(const QString& name) {
    if (name.isEmpty()) return name;

    QString out;
    out.reserve(name.size() + 8);
    out += name[0].toUpper();
    for (int i = 1; i < name.size(); ++i) {
        QChar c = name[i];
        if (c.isUpper() && !name[i - 1].isUpper())
            out += ' ';
        out += c;
    }
    return out;
}

static QString displayNameForProperty(const QString& name) {
    const QString alias = aliasForProperty(name);
    if (!alias.isEmpty()) return alias;
    return humanizeIdentifier(name);
}

// -- Importance ranking ---------------------------------------------------
// Within a section, properties are shown in the order Qt's meta-object
// happens to enumerate them, which is essentially arbitrary — so a widget's
// actual headline property (its text, its value, whether it's checked...)
// can end up buried under things like "cursor" or "whatsThis". This table
// gives commonly load-bearing properties an explicit rank; anything not
// listed falls back to importance() == the list's size, so unranked
// properties simply keep their original relative (meta-object) order,
// after all ranked ones. Extend this list as you notice something important
// still landing too low.
static int propertyImportance(const QString& name) {
    static const QVector<QString> important = {
        "text", "plainText", "html", "value", "checked", "currentText",
        "currentIndex", "title", "placeholderText", "pixmap", "icon",
        "font", "color", "minimum", "maximum", "singleStep", "range",
    };
    const int idx = important.indexOf(name);
    return idx >= 0 ? idx : important.size();
}

static QString sectionForProperty(const QString& name) {
    const QString n = name.toLower();

    auto has = [&n](std::initializer_list<const char*> keys) {
        for (const char* k : keys)
            if (n.contains(k)) return true;
        return false;
        };

    // Checked before "Text" on purpose: a name like "textColor" contains
    // both "text" and "color", and it's fundamentally a color, not prose —
    // whichever list runs first wins ties like that, so Appearance goes first.
    if (has({ "color", "colour", "font", "style", "icon", "pixmap",
              "opacity", "background", "border" }))
        return "Appearance";

    if (has({ "text", "title", "label", "placeholder", "caption" }))
        return "Text";

    if (has({ "size", "width", "height", "margin", "spacing", "geometry",
              "pos", "rect", "alignment" }))
        return "Geometry";

    if (has({ "checked", "checkable", "readonly", "editable", "toggle",
              "mode", "flag", "enabled", "visible" }))
        return "Behavior";

    return "General";
}
static bool hasRealColorProperty(const QVector<QMetaProperty>& props) {
    for (const auto& p : props) {
        const QString n = QString(p.name());
        if (n.contains("color", Qt::CaseInsensitive) || n.contains("colour", Qt::CaseInsensitive))
            return true;
    }
    return false;
}
WidgetEditorPage::WidgetEditorPage(QWidget* parent) : QWidget(parent) {
    setStyleSheet(QString("background:%1;").arg(Theme::BG));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->setSpacing(16);

    // -- Back button -------------------------------------------------------
    auto* backBtn = new QPushButton("\u2190 Back");
    backBtn->setFont(MF(9));
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFixedHeight(28);
    backBtn->setStyleSheet(QString(
        "QPushButton{background:none;border:1px solid %1;color:%2;"
        "border-radius:6px;padding:2px 12px;}"
        "QPushButton:hover{border-color:%3;color:%3;}"
    ).arg(Theme::BORDER, Theme::MUTED, Theme::ACCENT()));
    connect(backBtn, &QPushButton::clicked, this, &WidgetEditorPage::backClicked);

    auto* backRow = new QHBoxLayout;
    backRow->addWidget(backBtn, 0, Qt::AlignLeft);
    backRow->addStretch(1);
    root->addLayout(backRow);

    // -- Widget name — centered heading -------------------------------------
    m_nameLabel = new QLabel;
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setFont(MF(18, QFont::Bold));
    m_nameLabel->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::ACCENT()));
    root->addWidget(m_nameLabel);
    root->addSpacing(8);

    // -- Property search -----------------------------------------------------
    m_searchEdit = new QLineEdit;
    m_searchEdit->setPlaceholderText("Search properties\u2026");
    m_searchEdit->setFont(MF(9));
    m_searchEdit->setFixedHeight(30);
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setStyleSheet(QString(
        "QLineEdit{background:%1;border:1px solid %2;border-radius:6px;"
        "color:%3;padding:2px 10px;}"
        "QLineEdit:focus{border-color:%4;}"
    ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT, Theme::ACCENT()));
    connect(m_searchEdit, &QLineEdit::textChanged, this, &WidgetEditorPage::applyFilter);

    auto* searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(60, 0, 60, 0);
    searchRow->addWidget(m_searchEdit);
    root->addLayout(searchRow);
    root->addSpacing(4);

    // -- Scrollable property list -------------------------------------------
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background:transparent;border:none;");

    auto* propsContainer = new QWidget;
    propsContainer->setStyleSheet("background:transparent;");
    m_propsLayout = new QVBoxLayout(propsContainer);
    m_propsLayout->setContentsMargins(60, 0, 60, 0);
    m_propsLayout->setSpacing(8);

    m_emptyLabel = new QLabel("This widget has no editable properties.");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setFont(MF(10));
    m_emptyLabel->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::MUTED));
    m_emptyLabel->hide();
    m_propsLayout->addWidget(m_emptyLabel);
    m_propsLayout->addStretch(1);

    scroll->setWidget(propsContainer);
    root->addWidget(scroll, 1);
}

void WidgetEditorPage::inspect(QWidget* target) {
    m_target = target;
    rebuild();
}

// Filters the already-built property rows by a live search query, without
// needing to track any extra per-widget state: each row got a "filterKey"
// dynamic property (display name + raw name, lowercased) when it was built,
// and each section header got an "isSectionHeader" marker. Walking the
// layout in order and remembering the most recent header lets a single pass
// decide, retroactively, whether that header should stay visible — a header
// only makes sense to show if at least one of the rows under it survived
// the filter.
void WidgetEditorPage::applyFilter(const QString& query) {
    const QString q = query.trimmed().toLower();
    bool anyVisible = false;
    QWidget* pendingHeader = nullptr;
    bool pendingHeaderHasMatch = false;

    auto finalizePendingHeader = [&]() {
        if (pendingHeader) {
            pendingHeader->setVisible(pendingHeaderHasMatch);
            anyVisible |= pendingHeaderHasMatch;
        }
        };

    for (int i = 0; i < m_propsLayout->count(); ++i) {
        QLayoutItem* item = m_propsLayout->itemAt(i);
        QWidget* w = item->widget();
        if (!w || w == m_emptyLabel) continue; // trailing stretch / empty-state label

        if (w->property("isSectionHeader").toBool()) {
            finalizePendingHeader();
            pendingHeader = w;
            pendingHeaderHasMatch = false;
            continue;
        }

        const QString key = w->property("filterKey").toString();
        const bool match = q.isEmpty() || key.contains(q);
        w->setVisible(match);
        if (match) {
            pendingHeaderHasMatch = true;
            anyVisible = true;
        }
    }
    finalizePendingHeader();

    m_emptyLabel->setText(q.isEmpty()
        ? "This widget has no editable properties."
        : "No properties match your search.");
    m_emptyLabel->setVisible(!anyVisible);
}

QWidget* WidgetEditorPage::buildSectionHeader(const QString& title) {
    auto* row = new QWidget;
    row->setProperty("isSectionHeader", true);
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 12, 0, 4);
    rl->setSpacing(8);

    auto* lbl = new QLabel(title.toUpper());
    lbl->setFont(MF(8, QFont::Bold));
    lbl->setStyleSheet(QString("color:%1;background:transparent;letter-spacing:1px;")
        .arg(Theme::MUTED));
    rl->addWidget(lbl, 0);

    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFixedHeight(1);
    line->setStyleSheet(QString("background:%1;border:none;").arg(Theme::BORDER));
    rl->addWidget(line, 1);

    return row;
}

void WidgetEditorPage::rebuild() {
    // Clear existing rows — keep the trailing stretch + empty-state label,
    // which live at fixed positions (index 0 and the last item).
    while (m_propsLayout->count() > 2) {
        QLayoutItem* item = m_propsLayout->takeAt(1);
        delete item->widget();
        delete item;
    }

    if (!m_target) {
        m_nameLabel->setText("(no widget selected)");
        m_emptyLabel->show();
        return;
    }

    const QMetaObject* meta = m_target->metaObject();
    m_nameLabel->setText(meta->className());

    // -- Bucket properties into sections, preserving each property's
    // discovery order within its bucket. -----------------------------------
    QMap<QString, QVector<QMetaProperty>> buckets;
    for (int i = 0; i < meta->propertyCount(); ++i) {
        QMetaProperty prop = meta->property(i);
        if (!prop.isReadable()) continue;
        // Skip a few QWidget/QObject built-ins that are noisy and rarely
        // useful to hand-edit (geometry conflicts with the layout system,
        // and "objectName" is internal bookkeeping rather than user-facing).
        const QString name = prop.name();
        if (name == "geometry" || name == "pos" || name == "objectName")
            continue;
        // "enabled" and "visible" are structurally unsafe to expose here:
        // Qt drops mouse events for disabled/hidden widgets *before* any of
        // MasterLabel's own event handling runs (the check happens inside
        // QApplication::notify, ahead of both object-level event filters and
        // virtual overrides like mousePressEvent). Flipping either one off
        // through this editor would permanently remove the only way to
        // reopen it — there is no click that can reach a disabled or
        // invisible widget. See WidgetRegistry::forceReenableAll() for the
        // recovery path if a widget ever ends up stuck this way regardless.
        if (name == "enabled" || name == "visible")
            continue;

        buckets[sectionForProperty(name)].append(prop);
    }

    for (const QString& section : SECTION_ORDER) {
        auto it = buckets.find(section);
        QVector<QMetaProperty> props = (it != buckets.end()) ? it.value() : QVector<QMetaProperty>();
        std::stable_sort(props.begin(), props.end(),
            [](const QMetaProperty& a, const QMetaProperty& b) {
                return propertyImportance(a.name()) < propertyImportance(b.name());
            });

        // Only offer the styleSheet-based "Text Color" workaround if this
        // widget has no genuine color-named property already — otherwise
        // you'd see two rows claiming to control the same thing.
        const bool needsSyntheticTextColor = (section == "Appearance") && !hasRealColorProperty(props);
        if (props.isEmpty() && !needsSyntheticTextColor) continue;

        m_propsLayout->insertWidget(m_propsLayout->count() - 1, buildSectionHeader(section));

        if (needsSyntheticTextColor)
            m_propsLayout->insertWidget(m_propsLayout->count() - 1, buildStyleSheetTextColorRow(m_target));

        for (const QMetaProperty& prop : props) {
            QWidget* row = buildRowForProperty(prop, m_target);
            m_propsLayout->insertWidget(m_propsLayout->count() - 1, row);
        }
    }

    applyFilter(m_searchEdit->text());
}

static QString comboStyleSheet() {
    // The native drop-down arrow Qt draws by default is style-dependent and
    // often invisible against a dark theme, so we draw our own small chevron
    // with a CSS-triangle trick (no image resource needed) to make it clear
    // at a glance that these are dropdowns and not plain text fields.
    return QString(
        "QComboBox{background:%1;border:1px solid %2;border-radius:4px;"
        "color:%3;padding:2px 22px 2px 6px;}"
        "QComboBox::drop-down{subcontrol-origin:padding;subcontrol-position:top right;"
        "width:20px;border-left:1px solid %2;}"
        "QComboBox::down-arrow{width:0;height:0;margin-right:7px;"
        "border-left:4px solid transparent;border-right:4px solid transparent;"
        "border-top:5px solid %4;}"
    ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT, Theme::MUTED);
}

// -- "Text Color" via styleSheet --------------------------------------------
// Plenty of widgets (MasterLabel included) have no real color-typed
// property at all — their text color, if set, only exists as a "color: ...;"
// declaration buried in their styleSheet string. Rather than leave that
// invisible to the editor, we synthesize a "Text Color" row: it parses the
// current styleSheet for a standalone "color:" declaration (deliberately not
// "background-color:", hence the negative lookbehind), and writes any
// change straight back into the styleSheet string via setStyleSheet(). This
// only gets added when the widget doesn't already expose a genuine
// color-named property, so there's never a confusing duplicate.
static const QRegularExpression& styleColorRegex() {
    static const QRegularExpression re(
        R"((?<!-)\bcolor\s*:\s*([^;{}]+))",
        QRegularExpression::CaseInsensitiveOption);
    return re;
}

static QColor extractStyleTextColor(const QString& styleSheet) {
    const auto match = styleColorRegex().match(styleSheet);
    if (!match.hasMatch()) return QColor();
    return QColor(match.captured(1).trimmed());
}

static QString withStyleTextColor(const QString& styleSheet, const QColor& color) {
    const QString decl = QString("color: %1").arg(color.name());
    const auto match = styleColorRegex().match(styleSheet);
    if (match.hasMatch()) {
        QString out = styleSheet;
        out.replace(match.capturedStart(0), match.capturedLength(0), decl);
        return out;
    }
    // No existing "color:" declaration — append one. Most styleSheets in
    // this codebase are flat instance-level declarations with no selector
    // braces (e.g. "background:%1;border:1px solid %2;"), so just append;
    // if a brace block is present instead, insert just after the opening
    // brace so it lands inside the rule.
    const int braceIdx = styleSheet.indexOf('{');
    if (braceIdx >= 0) {
        QString out = styleSheet;
        out.insert(braceIdx + 1, " " + decl + ";");
        return out;
    }
    QString out = styleSheet.trimmed();
    if (!out.isEmpty() && !out.endsWith(';')) out += ';';
    if (!out.isEmpty()) out += ' ';
    out += decl + ';';
    return out;
}



QWidget* WidgetEditorPage::buildStyleSheetTextColorRow(QWidget* target) {
    auto* row = new QWidget;
    row->setProperty("filterKey", QString("text color textcolor stylesheet"));
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(12);

    auto* nameLbl = new QLabel("Text Color");
    nameLbl->setFont(MF(9));
    nameLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::MUTED));
    nameLbl->setToolTip("No real color property here — this edits \"color: ...\" "
        "inside the widget's styleSheet directly.");
    rl->addWidget(nameLbl, 1);

    auto* swatch = new QPushButton;
    swatch->setFixedSize(24, 24);
    swatch->setCursor(Qt::PointingHandCursor);
    swatch->setToolTip(nameLbl->toolTip());

    QColor c = extractStyleTextColor(target->styleSheet());
    if (!c.isValid()) c = QColor(Theme::MUTED); // no color set yet
    swatch->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:4px;")
        .arg(c.name(), Theme::BORDER));

    connect(swatch, &QPushButton::clicked, this, [target, swatch]() {
        QColor start = extractStyleTextColor(target->styleSheet());
        QColor chosen = QColorDialog::getColor(
            start.isValid() ? start : QColor(Qt::white), swatch, "Choose text color");
        if (chosen.isValid()) {
            target->setStyleSheet(withStyleTextColor(target->styleSheet(), chosen));
            swatch->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:4px;")
                .arg(chosen.name(), Theme::BORDER));
        }
        });
    rl->addWidget(swatch);

    return row;
}

QWidget* WidgetEditorPage::buildRowForProperty(const QMetaProperty& prop, QWidget* target) {
    auto* row = new QWidget;
    row->setProperty("filterKey",
        QString("%1 %2").arg(displayNameForProperty(prop.name()), QString(prop.name())).toLower());
    auto* rl = new QHBoxLayout(row);
    rl->setContentsMargins(0, 0, 0, 0);
    rl->setSpacing(12);

    auto* nameLbl = new QLabel(displayNameForProperty(prop.name()));
    nameLbl->setFont(MF(9));
    nameLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::MUTED));
    rl->addWidget(nameLbl, 1);

    const QString propName = prop.name();
    const QVariant current = target->property(propName.toUtf8().constData());
    const bool writable = prop.isWritable();

    if (!writable) {
        auto* valLbl = new QLabel(current.toString());
        valLbl->setFont(MF(9, QFont::Bold));
        valLbl->setStyleSheet(QString("color:%1;background:transparent;").arg(Theme::TEXT));
        valLbl->setAlignment(Qt::AlignRight);
        rl->addWidget(valLbl);
        return row;
    }

    // Enum/flag-backed properties (Qt::Alignment, QLineEdit::EchoMode,
    // Qt::CursorShape, QFrame::Shape, ...) report as QMetaType::Int/UInt
    // under the hood, which would otherwise land in the plain int spinbox
    // or, worse, the freeform QLineEdit below. Catch them first and present
    // their named values as a dropdown instead. Flag types (multiple bits
    // OR'd together) are simplified to single-select here for now — good
    // enough for the common single-value case, but combined flag values
    // will just show as whichever named value happens to match the bits.
    if (prop.isEnumType()) {
        QMetaEnum en = prop.enumerator();
        auto* combo = new QComboBox;
        combo->setFixedWidth(160);
        combo->setStyleSheet(comboStyleSheet());

        const int currentValue = current.toInt();
        int currentIndex = -1;
        for (int i = 0; i < en.keyCount(); ++i) {
            combo->addItem(en.key(i), en.value(i));
            if (en.value(i) == currentValue) currentIndex = i;
        }
        if (currentIndex >= 0) combo->setCurrentIndex(currentIndex);

        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [target, propName, combo](int idx) {
                const int value = combo->itemData(idx).toInt();
                target->setProperty(propName.toUtf8().constData(), value);
            });
        rl->addWidget(combo);
        return row;
    }

    // Some widgets store a color as a plain QString (e.g. "colorText"
    // holding a hex string like "#ff6b5b") rather than an actual QColor
    // property — probably for serialization convenience. Type-wise these
    // would otherwise fall through to the generic QLineEdit below, showing
    // raw hex text instead of something you can actually pick a color with.
    // Catch anything string-typed whose name says "color"/"colour" and give
    // it the same swatch + QColorDialog treatment as a real QColor property,
    // just serializing back to a string instead of a QColor value.
    if (current.typeId() == QMetaType::QString &&
        (propName.contains("color", Qt::CaseInsensitive) ||
            propName.contains("colour", Qt::CaseInsensitive))) {
        auto* swatch = new QPushButton;
        swatch->setFixedSize(24, 24);
        swatch->setCursor(Qt::PointingHandCursor);
        QColor c(current.toString());
        if (!c.isValid()) c = QColor(Theme::MUTED); // unparsable/empty text
        swatch->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:4px;")
            .arg(c.name(), Theme::BORDER));
        connect(swatch, &QPushButton::clicked, this, [target, propName, swatch]() {
            const QString raw = target->property(propName.toUtf8().constData()).toString();
            QColor start(raw);
            QColor chosen = QColorDialog::getColor(
                start.isValid() ? start : QColor(Qt::white), swatch, "Choose color");
            if (chosen.isValid()) {
                target->setProperty(propName.toUtf8().constData(), chosen.name());
                swatch->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:4px;")
                    .arg(chosen.name(), Theme::BORDER));
            }
            });
        rl->addWidget(swatch);
        return row;
    }

    switch (current.typeId()) {
    case QMetaType::Bool: {
        auto* box = new QCheckBox;
        box->setChecked(current.toBool());
        box->setStyleSheet(QString("color:%1;").arg(Theme::TEXT));
        connect(box, &QCheckBox::toggled, this, [target, propName](bool v) {
            target->setProperty(propName.toUtf8().constData(), v);
            });
        rl->addWidget(box);
        break;
    }
    case QMetaType::Int: {
        auto* edit = new QLineEdit(QString::number(current.toInt()));
        edit->setFixedWidth(120);
        edit->setFont(MF(9));
        edit->setStyleSheet(QString(
            "QLineEdit{background:%1;border:1px solid %2;border-radius:4px;"
            "color:%3;padding:2px 6px;}"
            "QLineEdit:focus{border-color:%4;}"
        ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT, Theme::ACCENT()));
        connect(edit, &QLineEdit::editingFinished, this, [target, propName, edit]() {
            bool ok = false;
            const double val = evaluateBigExpr(edit->text(), ok);
            if (ok && !std::isnan(val) && !std::isinf(val)) {
                const double clamped = qBound<double>(
                    static_cast<double>(std::numeric_limits<int>::min()), val,
                    static_cast<double>(std::numeric_limits<int>::max()));
                const int rounded = static_cast<int>(std::llround(clamped));
                target->setProperty(propName.toUtf8().constData(), rounded);
                edit->setText(QString::number(rounded));
            }
            else {
                // Invalid expression — revert to the property's last known value.
                edit->setText(QString::number(
                    target->property(propName.toUtf8().constData()).toInt()));
            }
            });
        rl->addWidget(edit);
        rl->addWidget(buildExprHintLabel());
        break;
    }
    case QMetaType::Double: {
        auto* edit = new QLineEdit(QString::number(current.toDouble(), 'g', 12));
        edit->setFixedWidth(140);
        edit->setFont(MF(9));
        edit->setStyleSheet(QString(
            "QLineEdit{background:%1;border:1px solid %2;border-radius:4px;"
            "color:%3;padding:2px 6px;}"
            "QLineEdit:focus{border-color:%4;}"
        ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT, Theme::ACCENT()));
        connect(edit, &QLineEdit::editingFinished, this, [target, propName, edit]() {
            bool ok = false;
            const double val = evaluateBigExpr(edit->text(), ok);
            if (ok && !std::isnan(val)) {
                target->setProperty(propName.toUtf8().constData(), val);
                edit->setText(QString::number(val, 'g', 12));
            }
            else {
                // Invalid expression — revert to the property's last known value.
                edit->setText(QString::number(
                    target->property(propName.toUtf8().constData()).toDouble(), 'g', 12));
            }
            });
        rl->addWidget(edit);
        rl->addWidget(buildExprHintLabel());
        break;
    }
    case QMetaType::QColor: {
        auto* swatch = new QPushButton;
        swatch->setFixedSize(24, 24);
        swatch->setCursor(Qt::PointingHandCursor);
        QColor c = current.value<QColor>();
        swatch->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:4px;")
            .arg(c.name(), Theme::BORDER));
        connect(swatch, &QPushButton::clicked, this, [target, propName, swatch]() {
            QColor start = target->property(propName.toUtf8().constData()).value<QColor>();
            QColor chosen = QColorDialog::getColor(start, swatch, "Choose color");
            if (chosen.isValid()) {
                target->setProperty(propName.toUtf8().constData(), chosen);
                swatch->setStyleSheet(QString("background:%1;border:1px solid %2;border-radius:4px;")
                    .arg(chosen.name(), Theme::BORDER));
            }
            });
        rl->addWidget(swatch);
        break;
    }
    case QMetaType::QFont: {
        // Only the family is exposed as a dropdown — point size, weight,
        // italics etc. stay whatever they were, we just swap the family in.
        auto* combo = new QFontComboBox;
        combo->setFixedWidth(180);
        combo->setStyleSheet(comboStyleSheet());
        combo->setCurrentFont(current.value<QFont>());
        connect(combo, &QFontComboBox::currentFontChanged, this,
            [target, propName](const QFont& chosen) {
                QFont f = target->property(propName.toUtf8().constData()).value<QFont>();
                f.setFamily(chosen.family());
                target->setProperty(propName.toUtf8().constData(), f);
            });
        rl->addWidget(combo);
        break;
    }
    default: {
        // Covers QString and anything else convertible to/from text.
        // Color-ish QStrings (colorText, textColor, ...) are handled above
        // with a real swatch; this is genuinely freeform text (formula,
        // arbitrary labels, etc).
        auto* edit = new QLineEdit(current.toString());
        edit->setFixedWidth(220);
        edit->setFont(MF(9));
        edit->setStyleSheet(QString(
            "QLineEdit{background:%1;border:1px solid %2;border-radius:4px;"
            "color:%3;padding:2px 6px;}"
            "QLineEdit:focus{border-color:%4;}"
        ).arg(Theme::SURFACE, Theme::BORDER, Theme::TEXT, Theme::ACCENT()));
        connect(edit, &QLineEdit::editingFinished, this, [target, propName, edit]() {
            target->setProperty(propName.toUtf8().constData(), edit->text());
            });
        rl->addWidget(edit);
        break;
    }
    }

    return row;
}