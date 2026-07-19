#pragma once
#include <QString>
#include <QStringList>
#include <QMap>
#include "ShapePrompt.h"

class GeoCard;

// Parsed `key=value` pairs from a shape expression. Values are already
// evaluated (so `b=2+2` yields 4.0), not raw text.
using ParamMap = QMap<QString, double>;

class InputHandler {
public:
    // -- Prompting -------------------------------------------------------------
    // A bare shape word ("circle", "tri") with no params: start collecting.
    static ShapePrompt detectPrompt(const QString& expr);

    // A shape expression that named a shape but omitted required params
    // ("tri a=3"): resume collecting from where it left off. Returns an
    // inactive prompt if `expr` isn't a shape, or is already complete.
    static ShapePrompt promptForMissingParams(const QString& expr);

    // Rebuild a full expression from a completed prompt.
    static QString buildExpr(const ShapePrompt& prompt);

    // -- Shape construction ----------------------------------------------------
    // nullptr if `expr` isn't a known shape or lacks required params.
    static GeoCard* makeGeoCard(const QString& expr);

    // -- Introspection ---------------------------------------------------------
    static QString     resolveAlias(const QString& word);
    static QStringList getRequiredParams(const QString& canonical);
    static QStringList missingRequiredParams(const QString& expr);

    // -- Parsing ---------------------------------------------------------------
    // Single source of truth for `key = value` extraction. Both makeGeoCard and
    // missingRequiredParams go through this, so they can never disagree about
    // whether `b=2+2` counts as "provided" -- which they used to, because each
    // carried its own regex.
    static ParamMap    parseParams(const QString& expr);
    static QStringList parseParamNames(const QString& expr);

    // Evaluates a parameter's right-hand side. Plain numbers fast-path;
    // anything else goes through MathEngine::evalSimple.
    static double evaluateParamValue(const QString& valueExpr, bool& ok);

private:
    // The canonical shape name in `expr`, or empty if it names no known shape.
    static QString commandOf(const QString& expr);

    // Does `provided` satisfy `required` for `shape`, directly or via a
    // substitute (d for r, ro for R)?
    static bool isSatisfied(const QString& required,
                            const QStringList& provided,
                            const struct Shape& shape);
};
