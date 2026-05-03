#include "InputHandler.h"
#include "../shapes/Shapes2D.h"
#include "../shapes/Shapes3D.h"
#include <QRegularExpression>
#include <cmath>
#include <functional>
#include <QMap>
#include "../shapes/ShapeDef.h"
#include <math/MathEngine.h>
// ----------------------------------------------------------------------
//  Shape definition table
// ----------------------------------------------------------------------
struct ShapeDef {
    QString     canonical;
    QStringList aliases;
    QStringList params;
    QStringList optional;
};

using ParamMap = QMap<QString, double>;
using CardFactory = std::function<GeoCard* (const ParamMap&)>;

static QMap<QString, CardFactory> cardFactories() {
    QMap<QString, CardFactory> factories;

    // ---------- 2D shapes ----------
    factories["circ"] = [](const ParamMap& p) -> GeoCard* {
        double r = p.value("r", 0);
        double d = p.value("d", 0);
        if (r == 0 && d > 0) r = d / 2;
        return (r > 0) ? new CircleCard(r) : nullptr;
        };
    factories["ellipse"] = [](const ParamMap& p) -> GeoCard* {
        double a = p.value("a", 0), b = p.value("b", 0);
        return (a > 0 && b > 0) ? new EllipseCard(a, b) : nullptr;
        };
    factories["tri"] = [](const ParamMap& p) -> GeoCard* {
        double a = p.value("a", 0), b = p.value("b", 0), c = p.value("c", 0);
        if (a > 0 && b > 0 && c == 0) c = std::sqrt(a * a + b * b);
        return (a > 0 && b > 0 && c > 0) ? new TriangleCard(a, b, c) : nullptr;
        };
    factories["righttri"] = [](const ParamMap& p) -> GeoCard* {
        double a = p.value("a", 0), b = p.value("b", 0);
        return (a > 0 && b > 0) ? new RightTriCard(a, b) : nullptr;
        };
    factories["rect"] = [](const ParamMap& p) -> GeoCard* {
        double w = p.value("w", 0), h = p.value("h", 0);
        return (w > 0 && h > 0) ? new RectCard(w, h) : nullptr;
        };
    factories["sqre"] = [](const ParamMap& p) -> GeoCard* {
        double s = p.value("s", 0);
        return (s > 0) ? new SquareCard(s) : nullptr;
        };
    factories["parallel"] = [](const ParamMap& p) -> GeoCard* {
        double b = p.value("b", 0), h = p.value("h", 0), s = p.value("s", 0);
        return (b > 0 && h > 0 && s > 0) ? new ParallelCard(b, h, s) : nullptr;
        };
    factories["trap"] = [](const ParamMap& p) -> GeoCard* {
        double a = p.value("a", 0), b = p.value("b", 0), h = p.value("h", 0);
        return (a > 0 && b > 0 && h > 0) ? new TrapezoidCard(a, b, h) : nullptr;
        };
    factories["rhombus"] = [](const ParamMap& p) -> GeoCard* {
        double d1 = p.value("d1", 0), d2 = p.value("d2", 0);
        return (d1 > 0 && d2 > 0) ? new RhombusCard(d1, d2) : nullptr;
        };
    factories["regpoly"] = [](const ParamMap& p) -> GeoCard* {
        int n = p.value("n", 0);
        double s = p.value("s", 0);
        return (n >= 3 && s > 0) ? new RegPolyCard(n, s) : nullptr;
        };
    factories["sector"] = [](const ParamMap& p) -> GeoCard* {
        double r = p.value("r", 0), angle = p.value("angle", 0);
        return (r > 0 && angle > 0) ? new SectorCard(r, angle) : nullptr;
        };
    factories["annulus"] = [](const ParamMap& p) -> GeoCard* {
        double R = p.value("major", 0), r = p.value("minor", 0);
        if (R == 0) R = p.value("major", 0);
        return (R > 0 && r > 0) ? new AnnulusCard(R, r) : nullptr;
        };

    // ---------- 3D shapes ----------
    factories["sphr"] = [](const ParamMap& p) -> GeoCard* {
        double r = p.value("r", 0);
        return (r > 0) ? new SphereCard(r) : nullptr;
        
        };
    factories["hemi"] = [](const ParamMap& p) -> GeoCard* {
        double r = p.value("r", 0);
        return (r > 0) ? new HemiSphereCard(r) : nullptr;
        };
    factories["cyld"] = [](const ParamMap& p) -> GeoCard* {
        double r = p.value("r", 0), h = p.value("h", 0);
        return (r > 0 && h > 0) ? new CylinderCard(r, h) : nullptr;
        };
    factories["hollcyl"] = [](const ParamMap& p) -> GeoCard* {
        double R = p.value("R", 0), r = p.value("r", 0), h = p.value("h", 0);
        if (R == 0) R = p.value("ro", 0);
        return (R > 0 && r > 0 && h > 0) ? new HollowCylCard(R, r, h) : nullptr;
        };
    factories["cone"] = [](const ParamMap& p) -> GeoCard* {
        double r = p.value("r", 0), h = p.value("h", 0);
        return (r > 0 && h > 0) ? new ConeCard(r, h) : nullptr;
        };
    factories["frustum"] = [](const ParamMap& p) -> GeoCard* {
        double R = p.value("R", 0), r = p.value("r", 0), h = p.value("h", 0);
        if (R == 0) R = p.value("ro", 0);
        return (R > 0 && r > 0 && h > 0) ? new FrustumCard(R, r, h) : nullptr;
        };
    factories["cuboid"] = [](const ParamMap& p) -> GeoCard* {
        double l = p.value("l", 0), w = p.value("w", 0), h = p.value("h", 0);
        return (l > 0 && w > 0 && h > 0) ? new CuboidCard(l, w, h) : nullptr;
        };
    factories["cube"] = [](const ParamMap& p) -> GeoCard* {
        double s = p.value("s", 0);
        return (s > 0) ? new CubeCard(s) : nullptr;
        };
    factories["tetra"] = [](const ParamMap& p) -> GeoCard* {
        double s = p.value("s", 0);
        return (s > 0) ? new TetraCard(s) : nullptr;
        };
    factories["octa"] = [](const ParamMap& p) -> GeoCard* {
        double s = p.value("s", 0);
        return (s > 0) ? new OctaCard(s) : nullptr;
        };
    factories["icosa"] = [](const ParamMap& p) -> GeoCard* {
        double s = p.value("s", 0);
        return (s > 0) ? new IcosaCard(s) : nullptr;
        };
    factories["dodeca"] = [](const ParamMap& p) -> GeoCard* {
        double s = p.value("s", 0);
        return (s > 0) ? new DodecaCard(s) : nullptr;
        };
    factories["prism"] = [](const ParamMap& p) -> GeoCard* {
        int n = p.value("n", 0);
        double s = p.value("s", 0), h = p.value("h", 0);
        return (n >= 3 && s > 0 && h > 0) ? new PrismCard(n, s, h) : nullptr;
        };
    factories["pyramid"] = [](const ParamMap& p) -> GeoCard* {
        double b = p.value("b", 0), h = p.value("h", 0);
        return (b > 0 && h > 0) ? new PyramidCard(b, h) : nullptr;
        };
    factories["torus"] = [](const ParamMap& p) -> GeoCard* {
        double R = p.value("major", 0), r = p.value("minor", 0);
        if (R == 0) R = p.value("major", 0);
        return (R > 0 && r > 0 && r < R) ? new TorusCard(R, r) : nullptr;
        };
    factories["ellipsoid"] = [](const ParamMap& p) -> GeoCard* {
        double a = p.value("a", 0), b = p.value("b", 0), c = p.value("c", 0);
        return (a > 0 && b > 0 && c > 0) ? new EllipsoidCard(a, b, c) : nullptr;
        };
    factories["capsule"] = [](const ParamMap& p) -> GeoCard* {
        double r = p.value("r", 0), h = p.value("h", 0);
        return (r > 0 && h > 0) ? new CapsuleCard(r, h) : nullptr;
        };

    return factories;
}

// ----------------------------------------------------------------------
//  detectPrompt - returns a prompt if the expression is a bare shape command
// ----------------------------------------------------------------------
ShapePrompt InputHandler::detectPrompt(const QString& expr) {
    // Build a set of all shape names and aliases (cached static)
    static const QSet<QString> knownShapes = []() {
        QSet<QString> set;
        for (const Shape& shape : ALL_SHAPES) {
            set.insert(shape.canonical);
            for (const QString& alias : shape.aliases)
                set.insert(alias);
        }
        return set;
        }();

    static QRegularExpression re(
        R"(^([a-zA-Z]+)\s*(?:\([^)0-9]*\))?\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(expr.trimmed());
    if (!m.hasMatch()) return {};

    QString word = m.captured(1).toLower();
    QString canonical = resolveAlias(word);
    if (canonical.isEmpty()) {
        if (knownShapes.contains(word)) canonical = word;
        else return {};
    }
    for (const Shape& shape : ALL_SHAPES) {
        if (shape.canonical == canonical) {
            ShapePrompt p;
            p.command = shape.canonical;
            p.params = shape.requiredParams;
            p.optional = shape.optionalParams;
            return p;
        }
    }
    return {};
}
// ----------------------------------------------------------------------
//  buildExpr - constructs a full expression from a completed prompt
// ----------------------------------------------------------------------
QString InputHandler::buildExpr(const ShapePrompt& prompt) {
    // Check if all required parameters have been collected
    bool allParamsFilled = true;
    for (const QString& p : prompt.params) {
        if (!prompt.collected.contains(p)) {
            allParamsFilled = false;
            break;
        }
    }

    // If all params are provided, omit the "(O)" placeholder
    QString cmd = prompt.command;
    if (!allParamsFilled) {
        if(cmd.contains("circ") || cmd.contains("sphere") || cmd.contains("sphr")) 
            cmd += "(O)";
    }

    QStringList parts;
    parts << cmd;
    for (const QString& param : prompt.params) {
        if (prompt.collected.contains(param))
            parts << QString("%1=%2").arg(param, prompt.collected[param]);
    }
    return parts.join(" ");
}

double InputHandler::evaluateParamValue(const QString& valueExpr, bool& ok) {
    // First try direct number conversion
    bool isNumber;
    double val = valueExpr.toDouble(&isNumber);
    if (isNumber) {
        ok = true;
        return val;
    }
    // Otherwise evaluate as expression
    val = MathEngine::evalSimple(valueExpr, ok);
    return val;
}

// ----------------------------------------------------------------------
//  makeGeoCard - creates an interactive card from a fully‑specified expression
// ----------------------------------------------------------------------
GeoCard* InputHandler::makeGeoCard(const QString& expr) {
    QString e = expr.trimmed().toLower();
    QString baseCmd = e.split('(').first().split(' ').first();
    QString canonical = resolveAlias(baseCmd);
    if (canonical.isEmpty()) return nullptr;

    ParamMap params;
    // Capture key and any non‑space value (including expressions)
    QRegularExpression re(R"(([a-z]+)\s*=\s*([^\s]+))");
    auto it = re.globalMatch(e);
    while (it.hasNext()) {
        auto m = it.next();
        QString key = m.captured(1).toLower();
        QString rawValue = m.captured(2);
        bool ok;
        double val = evaluateParamValue(rawValue, ok);
        if (ok) params[key] = val;
    }
    qDebug() << "expr:" << expr;
    qDebug() << "canonical:" << canonical;
    qDebug() << "params:" << params;
    static const QMap<QString, CardFactory> factories = cardFactories();
    if (factories.contains(canonical))
        return factories[canonical](params);

    return nullptr;
}

QStringList InputHandler::getRequiredParams(const QString& command) {
    for (const Shape& shape : ALL_SHAPES) {
        if (shape.canonical == command) {
            return shape.requiredParams;
        }
    }
    return {};
}
QStringList InputHandler::missingRequiredParams(const QString& expr) {

    QString e = expr.trimmed().toLower();
    QString baseCmd = e.split('(').first().split(' ').first();
    QString canonical = resolveAlias(baseCmd);
    if (canonical.isEmpty()) return {};

    // Parse provided parameters
    QSet<QString> provided;
    QRegularExpression re(R"(([a-z]+)\s*=\s*[\d.]+)");
    auto it = re.globalMatch(e);
    while (it.hasNext()) provided.insert(it.next().captured(1).toLower());

    for (const Shape& shape : ALL_SHAPES) {
        if (shape.canonical == canonical) {
            QStringList missing;
            for (const QString& req : shape.requiredParams) {
                if (!provided.contains(req)) missing << req;
            }
            return missing;
        }
    }
    return {};
}
// ----------------------------------------------------------------------
//  Helper: resolve alias to canonical name
// ----------------------------------------------------------------------

QString InputHandler::resolveAlias(const QString& word)
{ 
    QString w = word.toLower().remove(' ');
    for (const Shape& shape : ALL_SHAPES) {
        if (w == shape.canonical) return shape.canonical;
        for (const QString& alias : shape.aliases)
            if (w == alias) return shape.canonical;
    }
    return {};
}
