#pragma once
#include <QString>
#include "ShapePrompt.h"

class GeoCard;

class InputHandler {
public:
    static ShapePrompt detectPrompt(const QString& expr);
    static QString buildExpr(const ShapePrompt& prompt);
    static double evaluateParamValue(const QString& valueExpr, bool& ok);
    static GeoCard* makeGeoCard(const QString& expr);
    static QStringList missingRequiredParams(const QString& expr);
    static QString resolveAlias(const QString& word); 
    static QStringList getRequiredParams(const QString& command);
};