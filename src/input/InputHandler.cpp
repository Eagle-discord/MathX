#include "InputHandler.h"
#include "../shapes/Shapes2D.h"
#include "../shapes/Shapes3D.h"
#include "../shapes/ShapeDef.h"
#include <math/MathEngine.h>
#include <math/NaturalLanguage.h>
#include <QRegularExpression>
#include <QHash>
#include <QSet>
#include <cmath>
#include <functional>

// ----------------------------------------------------------------------
//  Lookup tables - built once, on first use
// ----------------------------------------------------------------------

// alias (and canonical) -> canonical. Replaces the O(shapes x aliases) linear
// scan that resolveAlias ran on every call, from three separate call sites.
static const QHash<QString, QString>& aliasMap() {
    static const QHash<QString, QString> map = []() {
        QHash<QString, QString> m;
        for (const Shape& s : ALL_SHAPES) {
            m.insert(s.canonical, s.canonical);
            for (const QString& a : s.aliases)
                m.insert(a, s.canonical);
        }
        return m;
    }();
    return map;
}

// canonical -> Shape. Same reason.
static const QHash<QString, const Shape*>& shapeMap() {
    static const QHash<QString, const Shape*> map = []() {
        QHash<QString, const Shape*> m;
        for (const Shape& s : ALL_SHAPES)
            m.insert(s.canonical, &s);
        return m;
    }();
    return map;
}

// The one regex. `([a-z0-9]+)` so `d1`/`d2` (rhombus) parse; the old
// `([a-z]+)` silently dropped the digit and produced a `d` key for both.
static const QRegularExpression& paramRegex() {
    static const QRegularExpression re(
        R"(([a-zA-Z][a-zA-Z0-9]*)\s*=\s*([^\s]+))");
    return re;
}

// ----------------------------------------------------------------------
//  Shape definition table -> card factories
// ----------------------------------------------------------------------
using CardFactory = std::function<GeoCard* (const ParamMap&)>;

// r from either `r` or `d` (diameter). Returns 0 if neither is present, which
// every caller checks. This used to be an uninitialized `double r;` read on the
// no-match path -- undefined behavior, and on MSVC debug builds a 0xCC-filled
// value that compares > 0 and reached the card constructor.
static double radiusFrom(const ParamMap& p) {
    if (p.contains("r")) return p.value("r");
    if (p.contains("d")) return p.value("d") / 2.0;
    return 0.0;
}

// Outer radius from either `R` or `ro`. Note QMap is case-sensitive, so `R`
// and `r` are distinct keys -- but makeGeoCard lowercases the expression before
// parsing, so `R` can never actually appear. See the note in makeGeoCard.
static double outerRadiusFrom(const ParamMap& p) {
    if (p.contains("R"))  return p.value("R");
    if (p.contains("ro")) return p.value("ro");
    return 0.0;
}

static int intFrom(const ParamMap& p, const QString& key) {
    return static_cast<int>(std::lround(p.value(key, 0)));
}

static const QMap<QString, CardFactory>& cardFactories() {
    static const QMap<QString, CardFactory> factories = []() {
        QMap<QString, CardFactory> f;

        // ---------- 2D shapes ----------
        f["circ"] = [](const ParamMap& p) -> GeoCard* {
            const double r = radiusFrom(p);
            return (r > 0) ? new CircleCard(r) : nullptr;
        };
        f["ellipse"] = [](const ParamMap& p) -> GeoCard* {
            const double a = p.value("a", 0), b = p.value("b", 0);
            return (a > 0 && b > 0) ? new EllipseCard(a, b) : nullptr;
        };
        f["tri"] = [](const ParamMap& p) -> GeoCard* {
            const double a = p.value("a", 0), b = p.value("b", 0);
            double c = p.value("c", 0);
            if (a > 0 && b > 0 && c == 0) c = std::sqrt(a * a + b * b);
            return (a > 0 && b > 0 && c > 0) ? new TriangleCard(a, b, c) : nullptr;
        };
        f["righttri"] = [](const ParamMap& p) -> GeoCard* {
            const double a = p.value("a", 0), b = p.value("b", 0);
            return (a > 0 && b > 0) ? new RightTriCard(a, b) : nullptr;
        };
        f["rect"] = [](const ParamMap& p) -> GeoCard* {
            const double w = p.value("w", 0), h = p.value("h", 0);
            return (w > 0 && h > 0) ? new RectCard(w, h) : nullptr;
        };
        f["sqre"] = [](const ParamMap& p) -> GeoCard* {
            const double s = p.value("s", 0);
            return (s > 0) ? new SquareCard(s) : nullptr;
        };
        f["parallel"] = [](const ParamMap& p) -> GeoCard* {
            const double b = p.value("b", 0), h = p.value("h", 0), s = p.value("s", 0);
            return (b > 0 && h > 0 && s > 0) ? new ParallelCard(b, h, s) : nullptr;
        };
        f["trap"] = [](const ParamMap& p) -> GeoCard* {
            const double a = p.value("a", 0), b = p.value("b", 0), h = p.value("h", 0);
            return (a > 0 && b > 0 && h > 0) ? new TrapezoidCard(a, b, h) : nullptr;
        };
        f["rhombus"] = [](const ParamMap& p) -> GeoCard* {
            const double d1 = p.value("d1", 0), d2 = p.value("d2", 0);
            return (d1 > 0 && d2 > 0) ? new RhombusCard(d1, d2) : nullptr;
        };
        f["regpoly"] = [](const ParamMap& p) -> GeoCard* {
            const int n = intFrom(p, "n");
            const double s = p.value("s", 0);
            return (n >= 3 && s > 0) ? new RegPolyCard(n, s) : nullptr;
        };
        f["sector"] = [](const ParamMap& p) -> GeoCard* {
            const double r = p.value("r", 0), angle = p.value("angle", 0);
            return (r > 0 && angle > 0) ? new SectorCard(r, angle) : nullptr;
        };
        f["annulus"] = [](const ParamMap& p) -> GeoCard* {
            const double R = p.value("major", 0), r = p.value("minor", 0);
            return (R > 0 && r > 0) ? new AnnulusCard(R, r) : nullptr;
        };

        // ---------- 3D shapes ----------
        f["sphr"] = [](const ParamMap& p) -> GeoCard* {
            const double r = radiusFrom(p);
            return (r > 0) ? new SphereCard(r) : nullptr;
        };
        f["hemi"] = [](const ParamMap& p) -> GeoCard* {
            const double r = radiusFrom(p);
            return (r > 0) ? new HemiSphereCard(r) : nullptr;
        };
        f["cyld"] = [](const ParamMap& p) -> GeoCard* {
            const double r = radiusFrom(p);
            const double h = p.value("h", 0);
            return (r > 0 && h > 0) ? new CylinderCard(r, h) : nullptr;
        };
        f["hollcyl"] = [](const ParamMap& p) -> GeoCard* {
            const double R = outerRadiusFrom(p);
            const double r = p.value("r", 0), h = p.value("h", 0);
            return (R > 0 && r > 0 && h > 0) ? new HollowCylCard(R, r, h) : nullptr;
        };
        f["cone"] = [](const ParamMap& p) -> GeoCard* {
            const double r = p.value("r", 0), h = p.value("h", 0);
            return (r > 0 && h > 0) ? new ConeCard(r, h) : nullptr;
        };
        f["frustum"] = [](const ParamMap& p) -> GeoCard* {
            const double R = outerRadiusFrom(p);
            const double r = p.value("r", 0), h = p.value("h", 0);
            return (R > 0 && r > 0 && h > 0) ? new FrustumCard(R, r, h) : nullptr;
        };
        f["cuboid"] = [](const ParamMap& p) -> GeoCard* {
            const double l = p.value("l", 0), w = p.value("w", 0), h = p.value("h", 0);
            return (l > 0 && w > 0 && h > 0) ? new CuboidCard(l, w, h) : nullptr;
        };
        f["cube"] = [](const ParamMap& p) -> GeoCard* {
            const double s = p.value("s", 0);
            return (s > 0) ? new CubeCard(s) : nullptr;
        };
        f["tetra"] = [](const ParamMap& p) -> GeoCard* {
            const double s = p.value("s", 0);
            return (s > 0) ? new TetraCard(s) : nullptr;
        };
        f["octa"] = [](const ParamMap& p) -> GeoCard* {
            const double s = p.value("s", 0);
            return (s > 0) ? new OctaCard(s) : nullptr;
        };
        f["icosa"] = [](const ParamMap& p) -> GeoCard* {
            const double s = p.value("s", 0);
            return (s > 0) ? new IcosaCard(s) : nullptr;
        };
        f["dodeca"] = [](const ParamMap& p) -> GeoCard* {
            const double s = p.value("s", 0);
            return (s > 0) ? new DodecaCard(s) : nullptr;
        };
        f["prism"] = [](const ParamMap& p) -> GeoCard* {
            const int n = intFrom(p, "n");
            const double s = p.value("s", 0), h = p.value("h", 0);
            return (n >= 3 && s > 0 && h > 0) ? new PrismCard(n, s, h) : nullptr;
        };
        f["pyramid"] = [](const ParamMap& p) -> GeoCard* {
            const double b = p.value("b", 0), h = p.value("h", 0);
            return (b > 0 && h > 0) ? new PyramidCard(b, h) : nullptr;
        };
        f["torus"] = [](const ParamMap& p) -> GeoCard* {
            const double R = p.value("major", 0), r = p.value("minor", 0);
            return (R > 0 && r > 0 && r < R) ? new TorusCard(R, r) : nullptr;
        };
        f["ellipsoid"] = [](const ParamMap& p) -> GeoCard* {
            const double a = p.value("a", 0), b = p.value("b", 0), c = p.value("c", 0);
            return (a > 0 && b > 0 && c > 0) ? new EllipsoidCard(a, b, c) : nullptr;
        };
        f["capsule"] = [](const ParamMap& p) -> GeoCard* {
            const double r = p.value("r", 0), h = p.value("h", 0);
            return (r > 0 && h > 0) ? new CapsuleCard(r, h) : nullptr;
        };

        return f;
    }();
    return factories;
}

// ----------------------------------------------------------------------
//  Alias / shape lookup
// ----------------------------------------------------------------------
QString InputHandler::resolveAlias(const QString& word) {
    return aliasMap().value(word.toLower().remove(' '));
}

QStringList InputHandler::getRequiredParams(const QString& canonical) {
    const Shape* s = shapeMap().value(canonical, nullptr);
    return s ? s->requiredParams : QStringList{};
}

// The canonical shape named at the head of `expr`, or empty.
QString InputHandler::commandOf(const QString& expr) {
    const QString head = expr.trimmed().toLower()
                             .split('(').first()
                             .split(' ').first();
    return resolveAlias(head);
}

// ----------------------------------------------------------------------
//  Parsing - one regex, one entry point
// ----------------------------------------------------------------------
ParamMap InputHandler::parseParams(const QString& expr) {
    ParamMap params;
    auto it = paramRegex().globalMatch(expr.toLower());
    while (it.hasNext()) {
        const auto m = it.next();
        bool ok = false;
        const double val = evaluateParamValue(m.captured(2), ok);
        if (ok) params[m.captured(1)] = val;
    }
    return params;
}

QStringList InputHandler::parseParamNames(const QString& expr) {
    QStringList names;
    auto it = paramRegex().globalMatch(expr.toLower());
    while (it.hasNext())
        names << it.next().captured(1);
    return names;
}

double InputHandler::evaluateParamValue(const QString& valueExpr, bool& ok) {
    bool isNumber = false;
    const double val = valueExpr.toDouble(&isNumber);
    if (isNumber) {
        ok = true;
        return val;
    }
    return MathEngine::evalSimple(valueExpr, ok);
}

// ----------------------------------------------------------------------
//  Requirement satisfaction
// ----------------------------------------------------------------------
bool InputHandler::isSatisfied(const QString& required,
                               const QStringList& provided,
                               const Shape& shape) {
    if (provided.contains(required)) return true;

    // A substitute counts: `d` satisfies `r`, `ro` satisfies `R`.
    for (const QString& sub : shape.substitutes.value(required))
        if (provided.contains(sub)) return true;

    return false;
}

QStringList InputHandler::missingRequiredParams(const QString& expr) {
    const QString canonical = commandOf(expr);
    if (canonical.isEmpty()) return {};

    const Shape* shape = shapeMap().value(canonical, nullptr);
    if (!shape) return {};

    // Names only. We don't care what the values evaluate to here -- a param
    // that fails to evaluate was still *provided*, and reporting it as missing
    // would restart the prompt on a typo instead of surfacing the error.
    const QStringList provided = parseParamNames(expr);

    QStringList missing;
    for (const QString& req : shape->requiredParams)
        if (!isSatisfied(req, provided, *shape))
            missing << req;

    return missing;
}

// ----------------------------------------------------------------------
//  detectPrompt - a bare shape word, no parameters
// ----------------------------------------------------------------------
ShapePrompt InputHandler::detectPrompt(const QString& expr) {
    static const QRegularExpression re(
        R"(^([a-zA-Z]+)\s*(?:\([^)0-9]*\))?\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    const auto m = re.match(expr.trimmed());
    if (!m.hasMatch()) return {};

    const QString canonical = resolveAlias(m.captured(1));
    if (canonical.isEmpty()) return {};

    const Shape* shape = shapeMap().value(canonical, nullptr);
    if (!shape) return {};

    ShapePrompt p;
    p.command  = shape->canonical;
    p.params   = shape->requiredParams;
    p.optional = shape->optionalParams;
    return p;
}

// ----------------------------------------------------------------------
//  promptForMissingParams - a shape expression that stopped short
// ----------------------------------------------------------------------
// Previously inlined in MainWindow::onWorkerFinish, and a second copy lived in
// the old run(). Both parsed with a regex that disagreed with makeGeoCard's.
ShapePrompt InputHandler::promptForMissingParams(const QString& expr) {
    ShapePrompt prompt;   // default-constructed => inactive

    const QString canonical = commandOf(expr);
    if (canonical.isEmpty()) return prompt;

    const Shape* shape = shapeMap().value(canonical, nullptr);
    if (!shape) return prompt;

    if (missingRequiredParams(expr).isEmpty()) return prompt;  // nothing to ask

    // Collect what's already there, as display strings. PromptController stores
    // collected values as text and buildExpr reassembles them, so we keep the
    // user's original spelling rather than round-tripping through double.
    QMap<QString, QString> prefill;
    auto it = paramRegex().globalMatch(expr.toLower());
    while (it.hasNext()) {
        const auto m = it.next();
        prefill[m.captured(1)] = m.captured(2);
    }

    // Resume at the first unsatisfied required param, not merely the first
    // absent one -- `sphr d=10` has `r` absent but satisfied, and must not
    // re-prompt for it.
    const QStringList providedNames = prefill.keys();
    int resumeAt = 0;
    for (const QString& p : shape->requiredParams) {
        if (!isSatisfied(p, providedNames, *shape)) break;
        ++resumeAt;
    }

    prompt.command      = shape->canonical;
    prompt.params       = shape->requiredParams;
    prompt.optional     = shape->optionalParams;
    prompt.currentIndex = resumeAt;
    prompt.collected    = prefill;
    return prompt;
}

// ----------------------------------------------------------------------
//  buildExpr - reassemble a completed prompt
// ----------------------------------------------------------------------
QString InputHandler::buildExpr(const ShapePrompt& prompt) {
    QStringList parts;
    parts << prompt.command;

    // Emit every collected value, required or optional. The old version only
    // walked prompt.params, so an optional the user supplied (d, c, ro) was
    // silently dropped from the rebuilt expression.
    for (auto it = prompt.collected.constBegin();
         it != prompt.collected.constEnd(); ++it) {
        parts << QString("%1=%2").arg(it.key(), it.value());
    }

    return NaturalLanguage::preprocess(parts.join(" "));
}

// ----------------------------------------------------------------------
//  makeGeoCard - build the card from a fully-specified expression
// ----------------------------------------------------------------------
GeoCard* InputHandler::makeGeoCard(const QString& expr) {
    const QString canonical = commandOf(expr);
    if (canonical.isEmpty()) return nullptr;

    // NOTE: parseParams lowercases, so `R` arrives as `r` and hollcyl/frustum
    // can never see a distinct outer radius under that name. `ro` is the
    // working spelling; outerRadiusFrom checks both so the behavior matches
    // whatever the parser happens to hand it.
    const QString normalized = NaturalLanguage::preprocess(expr.trimmed().toLower());
    const ParamMap params = parseParams(normalized);

    const auto& factories = cardFactories();
    const auto it = factories.constFind(canonical);
    return (it != factories.constEnd()) ? (*it)(params) : nullptr;
}
