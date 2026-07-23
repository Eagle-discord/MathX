// ----------------------------------------------------------------------
//  tryConversion - universal unit conversion system
//
//  Architecture:
//    1. SI_BASES  - one entry per physical quantity's base unit.
//                   Prefixable bases get all 20 SI prefixes generated
//                   automatically (km, mm, nm, Mm, etc.)
//
//    2. MANUAL    - non-SI units with known fixed factors, plus
//                   aliases, plural forms, and common abbreviations.
//
//    3. NONLINEAR - temperature and other offset/reciprocal units
//                   that can't be represented as a simple factor.
//
//    4. buildUnitMap() - called once, merges all three sources into a
//                   single QMap<QString, ResolvedUnit> for O(log n) lookup.
//
//  To add a new SI-derived unit:  one row in SI_BASES (prefixes are free).
//  To add a new non-SI unit:      one row in MANUAL.
//  To add a new non-linear unit:  one row in NONLINEAR.
//  Nothing else changes.
// ----------------------------------------------------------------------

#include <functional>
#include <cmath>
#include <qstring.h>
#include "MathEngine.h"
#include <qregularexpression.h>
#include "BigNum.h"


// -- Resolved unit -------------------------------------------------------------
struct ResolvedUnit {
    QString category;
    double  toBase = 0;
    std::function<double(double)> toBaseFn;
    std::function<double(double)> fromBaseFn;
    bool    isNonLinear = false;   // explicit flag for non‑linear units
    bool    valid = false;
    QString formula;   // human‑readable conversion explanation
};

// -- SI prefix table -----------------------------------------------------------
struct SIPrefix { QString sym; double factor; };
// Order matters: the input reaches unit resolution already lower-cased, which
// collapses the case-distinct SI prefixes M/m, P/p, Z/z, Y/y onto one key each.
// We list the everyday *small* prefix (milli, pico, zepto, yocto) BEFORE its
// uppercase partner (mega, peta, zetta, yotta) so the common unit wins - e.g.
// "mm" resolves to millimetre, not megametre. (Reach mega/peta/... by spelling
// the unit out, e.g. "megametre".) Non-colliding prefixes may appear in any order.
static const QVector<SIPrefix> SI_PREFIXES = {
    {"E",1e18},{"T",1e12},{"G",1e9},{"k",1e3},{"h",1e2},{"da",1e1},
    {"d",1e-1},{"c",1e-2},
    {"m",1e-3},{"u",1e-6},{"n",1e-9},{"p",1e-12},
    {"f",1e-15},{"a",1e-18},{"z",1e-21},{"y",1e-24},
    // Uppercase collision-partners last (their lower-cased key is already taken).
    {"M",1e6},{"P",1e15},{"Z",1e21},{"Y",1e24},
};

// -- SI base definitions -------------------------------------------------------
struct SIBase {
    QString     symbol;
    QString     category;
    double      toSI;
    bool        prefixable;
    QStringList aliases;
};
static const QVector<SIBase> SI_BASES = {
    {"m",   "length",           1.0,     true,  {"metre","meter","metres","meters"}},
    {"g",   "mass",             0.001,   true,  {"gram","grams"}},
    {"s",   "time",             1.0,     true,  {"sec","second","seconds"}},
    {"A",   "current",          1.0,     true,  {"ampere","amp","amps"}},
    // {"K",   "thermodynamic",    1.0,     true,  {"kelvin"}},   // removed (now in NONLINEAR)
    {"mol", "amount",           1.0,     true,  {"mole","moles"}},
    {"cd",  "luminosity",       1.0,     true,  {"candela"}},
    {"Hz",  "frequency",        1.0,     true,  {"hertz"}},
    {"N",   "force",            1.0,     true,  {"newton","newtons"}},
    {"Pa",  "pressure",         1.0,     true,  {"pascal","pascals"}},
    {"J",   "energy",           1.0,     true,  {"joule","joules"}},
    {"W",   "power",            1.0,     true,  {"watt","watts"}},
    {"Col",   "charge",           1.0,     true,  {"coulomb","coulombs"}},
    {"V",   "voltage",          1.0,     true,  {"volt","volts"}},
    {"Far",   "capacitance",      1.0,     true,  {"farad","farads"}},
    {"Ohm", "resistance",       1.0,     true,  {"ohm","ohms"}},
    {"S",   "conductance",      1.0,     true,  {"siemens"}},
    {"Wb",  "magnetic_flux",    1.0,     true,  {"weber","webers"}},
    {"T",   "magnetic_flux_density",1.0, true,  {"tesla","teslas"}},
    {"H",   "inductance",       1.0,     true,  {"henry","henries"}},
    {"lm",  "luminous_flux",    1.0,     true,  {"lumen","lumens"}},
    {"lx",  "illuminance",      1.0,     true,  {"lux"}},
    {"Bq",  "radioactivity",    1.0,     true,  {"becquerel"}},
    {"Gy",  "absorbed_dose",    1.0,     true,  {"gray"}},
    {"Sv",  "effective_dose",   1.0,     true,  {"sievert"}},
    {"kat", "catalytic",        1.0,     true,  {"katal"}},
    {"L",   "volume",           0.001,   true,  {"litre","liter","litres","liters"}},
    {"b",   "data",             1.0,     true,  {"byte","bytes"}},
};
// -- Manual non-SI units -------------------------------------------------------
struct ManualUnit {
    QString     key;
    QString     category;
    double      toBase;
    QStringList aliases;
};

static const QVector<ManualUnit> MANUAL = {
    // Length (base: m)
    {"mile",    "length", 1609.344,        {"miles"}},
    {"ft",      "length", 0.3048,          {"foot","feet"}},
    {"in",      "length", 0.0254,          {"inch","inches"}},
    {"yd",      "length", 0.9144,          {"yard","yards"}},
    {"thou",    "length", 2.54e-5,         {"thou","mil"}},
    {"nmi",     "length", 1852.0,          {"nauticalmile","nauticalmiles"}},
    {"furlong", "length", 201.168,         {"furlongs"}},
    {"chain",   "length", 20.1168,         {"chains"}},
    {"rod",     "length", 5.0292,          {"rods","perch"}},
    {"link",    "length", 0.201168,        {"links"}},
    {"fathom",  "length", 1.8288,          {"fathoms"}},
    {"league",  "length", 4828.032,        {"leagues"}},
    {"hand",    "length", 0.1016,          {"hands"}},
    {"cubit",   "length", 0.4572,          {"cubits"}},
    {"span",    "length", 0.2286,          {"spans"}},
    {"au",      "length", 1.495978707e11,  {"astronomicalunit"}},
    {"ly",      "length", 9.4607304726e15, {"lightyear","lightyears"}},
    {"pc",      "length", 3.0856775815e16, {"parsec","parsecs"}},
    {"angstrom","length", 1e-10,           {}},
    {"pica",    "length", 0.004233333,     {"picas"}},
    {"pt",      "length", 0.000352778,     {"point","points"}},
    // Mass (base: kg)
    {"lb",      "mass",   0.45359237,      {"lbs","pound","pounds"}},
    {"oz",      "mass",   0.028349523,     {"ounce","ounces"}},
    {"st",      "mass",   6.35029318,      {"stone","stones"}},
    {"dr",      "mass",   0.0017718452,    {"dram","drams"}},
    {"gr",      "mass",   6.47989e-5,      {"grain","grains"}},
    {"tonne",   "mass",   1000.0,          {"metric_ton","metric_tons"}},
    {"longton", "mass",   1016.0469088,    {"longtons"}},
    {"shortton","mass",   907.18474,       {"shorttons"}},
    {"cwt",     "mass",   50.80234544,     {"hundredweight"}},
    {"troyoz",  "mass",   0.031103477,     {"troyounce","ozt"}},
    {"troylb",  "mass",   0.373241722,     {"troypound"}},
    {"dwt",     "mass",   0.001555174,     {"pennyweight"}},
    {"carat",   "mass",   0.0002,          {"carats","ct"}},
    // Time (base: s)
    {"min",     "time",   60.0,            {"minute","minutes"}},
    {"hr",      "time",   3600.0,          {"hour","hours","h"}},
    {"day",     "time",   86400.0,         {"days","d"}},
    {"week",    "time",   604800.0,        {"weeks","wk"}},
    {"fortnight","time",  1209600.0,       {}},
    {"month",   "time",   2629800.0,       {"months","mo"}},
    {"year",    "time",   31557600.0,      {"years","yr","y"}},
    {"decade",  "time",   315576000.0,     {"decades"}},
    {"century", "time",   3155760000.0,    {"centuries"}},
    {"millennium","time", 3.15576e10,      {"millennia"}},
    // Volume (base: L, which is 0.001 m³ via SI_BASES)
    {"m3",      "volume", 1000.0,          {"cubicmetre","cubicmeter"}},
    {"ml",      "volume", 0.001,           {"millilitre","milliliter"}},
    {"cl",      "volume", 0.01,            {"centilitre"}},
    {"dl",      "volume", 0.1,             {"decilitre"}},
    {"gal",     "volume", 3.785411784,     {"gallon","gallons","usgal"}},
    {"ukgal",   "volume", 4.54609,         {"imperialgallon"}},
    {"qt",      "volume", 0.946352946,     {"quart","quarts"}},
    {"pint",    "volume", 0.473176473,     {"pints"}},
    {"cup",     "volume", 0.2365882365,    {"cups"}},
    {"floz",    "volume", 0.0295735296,    {"fl_oz","fluidounce"}},
    {"tbsp",    "volume", 0.0147867648,    {"tablespoon","tablespoons"}},
    {"tsp",     "volume", 0.0049289216,    {"teaspoon","teaspoons"}},
    {"bbl",     "volume", 158.987295,      {"barrel","barrels"}},
    {"bushel",  "volume", 35.23907,        {"bushels","bu"}},
    {"peck",    "volume", 8.809768,        {"pecks"}},
    // Area (base: m²)
    {"ha",      "area",   10000.0,         {"hectare","hectares"}},
    {"are",     "area",   100.0,           {"ares"}},
    {"acre",    "area",   4046.8564224,    {"acres"}},
    {"sqmi",    "area",   2589988.110336,  {"squaremile","squaremiles"}},
    {"sqft",    "area",   0.09290304,      {"squarefoot","squarefeet"}},
    {"sqin",    "area",   0.00064516,      {"squareinch","squareinches"}},
    {"sqyd",    "area",   0.83612736,      {"squareyard","squareyards"}},
    {"sqkm",    "area",   1e6,             {"squarekm"}},
    {"rood",    "area",   1011.7141056,    {"roods"}},
    // Pressure (base: Pa)
    {"bar",     "pressure",100000.0,       {"bars"}},
    {"mbar",    "pressure",100.0,          {"millibar","millibars"}},
    {"atm",     "pressure",101325.0,       {"atmosphere","atmospheres"}},
    {"torr",    "pressure",133.3223684,    {"torrs"}},
    {"mmhg",    "pressure",133.3223684,    {}},
    {"psi",     "pressure",6894.757293,    {}},
    {"ksi",     "pressure",6894757.293,    {}},
    {"inhg",    "pressure",3386.389,       {}},
    // Energy (base: J)
    {"cal",     "energy", 4.184,           {"calorie","calories"}},
    {"kcal",    "energy", 4184.0,          {"kilocalorie","kilocalories","Cal"}},
    {"wh",      "energy", 3600.0,          {"watthour"}},
    {"kwh",     "energy", 3600000.0,       {"kilowatthour"}},
    {"mwh",     "energy", 3.6e9,           {"megawatthour"}},
    {"btu",     "energy", 1055.05585,      {}},
    {"therm",   "energy", 105480400.0,     {"therms"}},
    {"erg",     "energy", 1e-7,            {"ergs"}},
    {"ftlb",    "energy", 1.355817948,     {"footpound"}},
    {"ev",      "energy", 1.602176634e-19, {"electronvolt"}},
    {"kev",     "energy", 1.602176634e-16, {}},
    {"mev",     "energy", 1.602176634e-13, {}},
    {"gev",     "energy", 1.602176634e-10, {}},
    // Power (base: W)
    {"hp",      "power",  745.69987,       {"horsepower"}},
    {"ps",      "power",  735.49875,       {"metrichorsepower"}},
    // Force (base: N)
    {"lbf",     "force",  4.4482216,       {"poundforce"}},
    {"kgf",     "force",  9.80665,         {"kilogramforce"}},
    {"dyn",     "force",  1e-5,            {"dyne","dynes"}},
    {"pdl",     "force",  0.13825495,      {"poundal"}},
    // Speed (base: m/s)
    { "celeritas", "speed", 299792458.0, {"lightspeed", "speed of light", "times the speed of light"}},
    { "mph",     "speed",  0.44704,         {"miles per hour","mile per hour","mileperhour","milesperhour"} },
    { "kph",     "speed",  1.0 / 3.6,         {"km/h","kmh","kmph","kilometers per hour","kilometres per hour","kilometer per hour","kilometre per hour"} },
    { "knot",    "speed",  0.51444444,      {"knots","kn","kt","nautical mile per hour","nautical miles per hour"} },
    { "mach",    "speed",  343.0,           {"mach speed","speed of sound","times the speed of sound","times mach"} },
    { "m/s",     "speed",  1.0,             {"meters per second","metres per second","meter per second","metre per second","meterspersecond","metrespersecond"} },
    { "km/h",    "speed",  1.0 / 3.6,         {"kilometers per hour","kilometres per hour"} },
    { "ft/s",    "speed",  0.3048,          {"feet per second","foot per second","fps"} },
    {"knot",    "speed",  0.51444444,      {"knots","kn","kt"}},
    {"fps",     "speed",  0.3048,          {"ft/s"}},
    {"mach",    "speed",  343.0,           {"mach speed", "speed of sound", "times the speed of sound", "times mach speed"}},
    // Frequency (base: Hz)
    {"rpm",     "frequency",1.0 / 60.0,      {}},
    {"rps",     "frequency",1.0,           {}},
    // Data (base: byte) - SI decimal
    // Data
    { "kb",      "data",   1000.0,          {"kilobyte","kilobytes","kilo byte","kilo bytes"} },
    { "mb",      "data",   1e6,             {"megabyte","megabytes","mega byte","mega bytes"} },
    { "gb",      "data",   1e9,             {"gigabyte","gigabytes","giga byte","giga bytes"} },
    { "tb",      "data",   1e12,            {"terabyte","terabytes","tera byte","tera bytes"} },
    { "pb",      "data",   1e15,            {"petabyte","petabytes"} },
    { "kib",     "data",   1024.0,          {"kibibyte","kibibytes"} },
    { "mib",     "data",   1048576.0,       {"mebibyte","mebibytes"} },
    { "gib",     "data",   1073741824.0,    {"gibibyte","gibibytes"} },
    // Data - binary IEC
    {"kib",     "data",   1024.0,          {"kibibyte"}},
    {"mib",     "data",   1048576.0,       {"mebibyte"}},
    {"gib",     "data",   1073741824.0,    {"gibibyte"}},
    {"tib",     "data",   1099511627776.0, {"tebibyte"}},
    // Angle (base: degree)
    {"deg",     "angle",  1.0,             {"degree","degrees"}},
    {"rad",     "angle",  180.0 / M_PI,      {"radian","radians"}},
    {"grad",    "angle",  0.9,             {"gradian","gon"}},
    {"arcmin",  "angle",  1.0 / 60.0,        {"arcminute"}},
    {"arcsec",  "angle",  1.0 / 3600.0,      {"arcsecond"}},
    {"turn",    "angle",  360.0,           {"revolution","rev"}},
    // Radiation
    {"rem",     "effective_dose",0.01,     {"rems"}},
    {"ci",      "radioactivity",3.7e10,    {"curie","curies"}},
    // Luminance
    {"fc",      "illuminance",10.7639104,  {"footcandle","footcandles"}},
    // Magnetic
    {"gauss",   "magnetic_flux_density",1e-4,{"G"}},
    {"oe",      "magnetic_field",79.5774715,{"oersted"}},
    {"mx",      "magnetic_flux",1e-8,      {"maxwell"}},
    // Viscosity
    {"poise",   "dynamic_viscosity",0.1,   {"P"}},
    {"cp",      "dynamic_viscosity",0.001, {"centipoise"}},
    {"stokes",  "kinematic_viscosity",1e-4,{"St"}},
    {"cst",     "kinematic_viscosity",1e-6,{"centistokes"}},
    // Torque (base: N·m)
    {"ftlbf",   "torque", 1.355817948,     {"footpoundforce"}},
    {"inlbf",   "torque", 0.112984829,     {"inchpoundforce"}},
    {"kgfm",    "torque", 9.80665,         {}},
    // Fuel economy (base: km/L)
    {"mpg",     "fueleco",0.425143707,     {"mpgus"}},
    {"mpguk",   "fueleco",0.354006180,     {}},
    {"kml",     "fueleco",1.0,             {"km/l"}},
};

// -- Non-linear units ----------------------------------------------------------
struct NonLinearUnit {
    QString     key;
    QString     category;
    std::function<double(double)> toBase;
    std::function<double(double)> fromBase;
    QStringList aliases;
};

static const QVector<NonLinearUnit> NONLINEAR = {
    // Internal base: Kelvin
     {"c",  "temperature",
        [](double v) { return v + 273.15; },               // Celsius → Kelvin
        [](double v) { return v - 273.15; },               // Kelvin → Celsius
        {"celsius","degc", "degrees celsius", "degrees celcius"}},

    {"f",  "temperature",
        [](double v) { return (v - 32.0) * 5.0/9.0 + 273.15; }, // °F → K
        [](double v) { return (v - 273.15) * 9.0/5.0 + 32.0; }, // K → °F
        {"fahrenheit","degf", "degrees fahrenheit", "°F", "° Fahrenheit"}}, /*very unlikely, but handle it anyway*/
    {"k",  "temperature",
        [](double v) { return v; },                        // K → K
        [](double v) { return v; },                        // K → K
        {"kelvin","degk"}},
    {"r",  "temperature",
        [](double v) { return v * 5.0/9.0; },              // Rankine → K
        [](double v) { return v * 9.0/5.0; },              // K → Rankine
        {"rankine","degr"}},
    {"delisle", "temperature",
        [](double v) { return 373.15 - v * 2.0 / 3.0; },     // Delisle → K
        [](double v) { return (373.15 - v) * 3.0 / 2.0; },   // K → Delisle
        {"delisle"}},
    {"nn", "temperature",
        [](double v) { return v * 100.0 / 33.0 + 273.15; },  // Newton → K
        [](double v) { return (v - 273.15) * 33.0 / 100.0; },// K → Newton
        {"newtonscale"}},
    {"reamur", "temperature",
        [](double v) { return v * 5.0 / 4.0 + 273.15; },     // Réaumur → K
        [](double v) { return (v - 273.15) * 4.0 / 5.0; },   // K → Réaumur
        {"reaumur"}},
    {"ro", "temperature",
        [](double v) { return (v - 7.5) * 40.0 / 21.0 + 273.15; }, // Rømer → K
        [](double v) { return (v - 273.15) * 21.0 / 40.0 + 7.5; },  // K → Rømer
        {"romer"}},
        // L/100km - reciprocal of km/L
        {"l/100km","fueleco",
            [](double v) { return 100.0 / v; },
            [](double v) { return 100.0 / v; },
            {"lper100km","l100km"}},
};

// -- Build lookup map (once) ---------------------------------------------------
static QMap<QString, ResolvedUnit> buildUnitMap() {
    QMap<QString, ResolvedUnit> map;

    auto addLinear = [&](const QString& rawKey, const QString& cat, double toBase) {
        QString k = rawKey.toLower().trimmed().simplified(); // normalize spaces
        if (!map.contains(k))
            map[k] = { cat, toBase, {}, {}, false, true };
        };

    auto addNonLinear = [&](const QString& rawKey, const QString& cat,
        std::function<double(double)> toFn,
        std::function<double(double)> fromFn) {
            QString k = rawKey.toLower().trimmed().simplified(); // normalize spaces
            if (!map.contains(k))
                map[k] = { cat, 0, toFn, fromFn, true, true };
        };

    // 1. Non-linear units (temperature etc.) - highest priority
    for (const NonLinearUnit& u : NONLINEAR) {
        addNonLinear(u.key, u.category, u.toBase, u.fromBase);
        for (const QString& alias : u.aliases)
            addNonLinear(alias, u.category, u.toBase, u.fromBase);
    }

    // 2. Manual non-SI units - takes precedence over auto‑generated ones
    for (const ManualUnit& u : MANUAL) {
        addLinear(u.key, u.category, u.toBase);
        for (const QString& alias : u.aliases)
            addLinear(alias, u.category, u.toBase);
    }

    // 3. SI bases + prefixes - only add if not already present
    for (const SIBase& base : SI_BASES) {
        // Add the base unit itself (if not already taken)
        if (!map.contains(base.symbol.toLower()))
            addLinear(base.symbol, base.category, base.toSI);
        for (const QString& alias : base.aliases) {
            if (!map.contains(alias.toLower()))
                addLinear(alias, base.category, base.toSI);
        }

        if (!base.prefixable) continue;

        // Add all prefixed forms
        for (const SIPrefix& pfx : SI_PREFIXES) {
            double factor = base.toSI * pfx.factor;
            QString prefixedKey = pfx.sym + base.symbol;
            if (!map.contains(prefixedKey.toLower()))
                addLinear(prefixedKey, base.category, factor);

            // Also add prefixed aliases (short ones only)
            for (const QString& alias : base.aliases) {
                if (alias.length() <= 6) {
                    QString prefixedAlias = pfx.sym + alias;
                    if (!map.contains(prefixedAlias.toLower()))
                        addLinear(prefixedAlias, base.category, factor);
                }
            }
        }
    }
    static const QMap<QString, QString> SI_PREFIX_WORDS = {
    {"Y","yotta"},{"Z","zetta"},{"E","exa"},{"P","peta"},
    {"T","tera"}, {"G","giga"},{"M","mega"},{"k","kilo"},
    {"h","hecto"},{"da","deca"},{"d","deci"},{"c","centi"},
    {"m","milli"},{"u","micro"},{"n","nano"},{"p","pico"},
    {"f","femto"},{"a","atto"},{"z","zepto"},{"y","yocto"}
    };

    for (const SIBase& base : SI_BASES) {
        if (!base.prefixable) continue;
        for (const SIPrefix& pfx : SI_PREFIXES) {
            double factor = base.toSI * pfx.factor;
            QString wordPrefix = SI_PREFIX_WORDS.value(pfx.sym);
            if (wordPrefix.isEmpty()) continue;

            // For each base alias, generate "kilo+alias" and "kilo+alias+s"
            for (const QString& alias : base.aliases) {
                QString wordForm = wordPrefix + alias;         // "kilogram"
                QString wordForms = wordPrefix + alias + "s";  // "kilograms"
                if (!map.contains(wordForm.toLower()))
                    addLinear(wordForm, base.category, factor);
                if (!map.contains(wordForms.toLower()))
                    addLinear(wordForms, base.category, factor);
            }
            // Also try symbol-based: "kilometre", "kilometres", "kilometer", "kilometers"
            for (const QString& alias : base.aliases) {
                // handle metre/meter duality
                if (alias.endsWith("re")) {
                    QString erForm = wordPrefix + alias;
                    QString erForms = wordPrefix + alias + "s";
                    QString erForm2 = wordPrefix + QString(alias).replace("re", "er");
                    QString erForms2 = wordPrefix + QString(alias).replace("re", "er") + "s";
                    for (auto& w : { erForm, erForms, erForm2, erForms2 }) {
                        if (!map.contains(w.toLower()))
                            addLinear(w, base.category, factor);
                    }
                }
            }
        }
    }
    return map;
}

struct UnitComponent {
    QString numUnit;
    QString denomUnit;
    bool isCompound = false;
};

struct CompoundUnit {
    QString numerator;
    QString denominator;
    bool isValid = false;


};

static CompoundUnit parseCompoundUnit(const QString& s) {
    CompoundUnit result;
    if (s.contains('/')) {
        QStringList parts = s.split('/');
        if (parts.size() == 2) {
            result.numerator = parts[0].trimmed().toLower();
            result.denominator = parts[1].trimmed().toLower();
            result.isValid = true;
        }
    }
    return result;
}

static QString defaultUnitForCategory(const QString& category) {
    static QMap<QString, QString> defaults = {
        {"length", "m"},
        {"mass", "kg"},
        {"time", "s"},
        {"speed", "m/s"},      // for compound units like km/h
        {"temperature", "c"},  // Celsius as default
        {"current", "A"},
        {"amount", "mol"},
        {"luminosity", "cd"},
        {"frequency", "Hz"},
        {"force", "N"},
        {"pressure", "Pa"},
        {"energy", "J"},
        {"power", "W"},
        {"charge", "C"},
        {"voltage", "V"},
        {"capacitance", "F"},
        {"resistance", "Ohm"},
        {"conductance", "S"},
        {"magnetic_flux", "Wb"},
        {"magnetic_flux_density", "T"},
        {"inductance", "H"},
        {"luminous_flux", "lm"},
        {"illuminance", "lx"},
        {"radioactivity", "Bq"},
        {"absorbed_dose", "Gy"},
        {"effective_dose", "Sv"},
        {"catalytic", "kat"},
        {"volume", "L"},
        {"data", "b"},
        {"angle", "deg"},
        {"fueleco", "km/L"},
    };
    return defaults.value(category, QString());
}

static QString getTemperatureFormula(const QString& from, const QString& to) {
    if (from == "c" && to == "f") return "°F = (°C × 9/5) + 32";
    if (from == "f" && to == "c") return "°C = (°F - 32) × 5/9";
    if (from == "c" && to == "k") return "K = °C + 273.15";
    if (from == "k" && to == "c") return "°C = K - 273.15";
    if (from == "f" && to == "k") return "K = (°F - 32) × 5/9 + 273.15";
    if (from == "k" && to == "f") return "°F = (K - 273.15) × 9/5 + 32";
    // fallback: factor (though not meaningful for temperature)
    return {};
}
// Add after the existing SI prefix loop
static const QMap<QString, QString> SI_PREFIX_WORDS = {
    {"Y","yotta"},{"Z","zetta"},{"E","exa"},{"P","peta"},
    {"T","tera"}, {"G","giga"},{"M","mega"},{"k","kilo"},
    {"h","hecto"},{"da","deca"},{"d","deci"},{"c","centi"},
    {"m","milli"},{"u","micro"},{"n","nano"},{"p","pico"},
    {"f","femto"},{"a","atto"},{"z","zepto"},{"y","yocto"}
};


// ----------------------------------------------------------------------
// Helper: convert simple units (no '/') - handles linear and non‑linear
// ----------------------------------------------------------------------
static CalcResult convertSimpleUnit(double val, const QString& fromUnit, const QString& toUnit,
    const QMap<QString, ResolvedUnit>& MAP) {
    if (!MAP.contains(fromUnit) || !MAP.contains(toUnit)) return {};
    const ResolvedUnit& uFrom = MAP[fromUnit];
    const ResolvedUnit& uTo = MAP[toUnit];
    if (uFrom.category != uTo.category) return {};

    double result;
    QString formula;

    // Non‑linear units (temperature) - use stored conversion functions
    if (uFrom.isNonLinear || uTo.isNonLinear) {
        double base = uFrom.toBaseFn ? uFrom.toBaseFn(val) : val * uFrom.toBase;
        result = uTo.fromBaseFn ? uTo.fromBaseFn(base) : base / uTo.toBase;
        // Try to get a human‑readable formula (only for temperature)
        formula = getTemperatureFormula(fromUnit, toUnit);
        if (formula.isEmpty()) {
            // Fallback factor (should not happen for known temperatures)
            formula = QString("1 %1 = %2 %3").arg(fromUnit).arg(uFrom.toBase / uTo.toBase).arg(toUnit);
        }
    }
    else {
        // Linear conversion
        result = val * uFrom.toBase / uTo.toBase;
        double factor = uFrom.toBase / uTo.toBase;
        formula = QString("1 %1 = %2 %3").arg(fromUnit).arg(factor).arg(toUnit);
    }

    if (std::isnan(result) || std::isinf(result)) return {};

    return {
        QString("%1 %2 = %3 %4").arg(val).arg(fromUnit).arg(BigNum::fmt(result)).arg(toUnit),
        ResultType::conv,
        formula
    };
}

// ----------------------------------------------------------------------
// tryConversion - main entry point
// ----------------------------------------------------------------------
CalcResult MathEngine::tryConversion(const QString& expr) {
    static const QMap<QString, ResolvedUnit> MAP = buildUnitMap();



    // Helper to get default target unit for a category
    auto defaultUnitForCategory = [](const QString& category) -> QString {
        static QMap<QString, QString> defaults = {
            {"length", "m"}, {"mass", "kg"}, {"time", "s"},
            {"speed", "m/s"}, {"temperature", "c"}, {"current", "A"},
            {"amount", "mol"}, {"luminosity", "cd"}, {"frequency", "Hz"},
            {"force", "N"}, {"pressure", "Pa"}, {"energy", "J"},
            {"power", "W"}, {"charge", "C"}, {"voltage", "V"},
            {"capacitance", "F"}, {"resistance", "Ohm"}, {"conductance", "S"},
            {"magnetic_flux", "Wb"}, {"magnetic_flux_density", "T"},
            {"inductance", "H"}, {"luminous_flux", "lm"}, {"illuminance", "lx"},
            {"radioactivity", "Bq"}, {"absorbed_dose", "Gy"}, {"effective_dose", "Sv"},
            {"catalytic", "kat"}, {"volume", "L"}, {"data", "b"},
            {"angle", "deg"}, {"fueleco", "km/L"}
        };
        return defaults.value(category);
        };

    // Special case: "mach X" -> "X mach"
    static QRegularExpression machRe(R"(\bmach\s+([\d.]+)\b)", QRegularExpression::CaseInsensitiveOption);
    QString processed = expr;
    processed.replace(machRe, "\\1 mach");

    // Raw unit conversion: "value unit" (no "to")
    static QRegularExpression rawRe(
        R"(^\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s+([A-Za-z0-9/]+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto rawMatch = rawRe.match(processed.trimmed());
    if (rawMatch.hasMatch()) {
        double val = rawMatch.captured(1).toDouble();
        QString fromUnit = rawMatch.captured(2).toLower();

        // Try simple unit first
        if (MAP.contains(fromUnit)) {
            const ResolvedUnit& uFrom = MAP[fromUnit];
            QString defaultTarget = defaultUnitForCategory(uFrom.category);
            if (!defaultTarget.isEmpty() && MAP.contains(defaultTarget)) {
                const ResolvedUnit& uTo = MAP[defaultTarget];
                if (uFrom.category == uTo.category) {
                    // Use the same helper for raw conversion
                    return convertSimpleUnit(val, fromUnit, defaultTarget, MAP);
                }
            }
        }
        // If simple unit fails, try compound unit (speed only for now)
        CompoundUnit fromComp = parseCompoundUnit(fromUnit);
        if (fromComp.isValid) {
            if (fromComp.numerator == "km" && fromComp.denominator == "h") {
                double factor = 1000.0 / 3600.0;
                double result = val * factor;
                return {
                    QString("%1 %2 = %3 m/s").arg(val).arg(fromUnit).arg(BigNum::fmt(result)),
                    ResultType::conv,
                    QString("1 %1 = %2 m/s").arg(fromUnit).arg(factor)
                };
            }
        }
        return {};
    }

    // Standard conversion pattern: "value unit to unit"
// Allow letters, digits, spaces, slashes, dots, hyphens - but NOT "to" as standalone word
    static QRegularExpression re(
        R"(^\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s+([\w/.*\- ]+?)\s+to\s+([\w/.*\- ]+?)\s*$)",
        QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(processed.trimmed());
    if (!m.hasMatch()) return {};

    double val = m.captured(1).toDouble();
    QString fromUnit = m.captured(2).toLower().trimmed().simplified();
    QString toUnit = m.captured(3).toLower().trimmed().simplified();

    // Simple units (no '/') - use dedicated helper
    if (!fromUnit.contains('/') && !toUnit.contains('/')) {
        return convertSimpleUnit(val, fromUnit, toUnit, MAP);
    }

    // --------------------------------------------------------------
    // Compound units (contain '/') - use factor method (linear only)
    // --------------------------------------------------------------
    auto parseUnitExpr = [](const QString& s) -> QPair<QStringList, QStringList> {
        if (s.contains('/')) {
            QStringList parts = s.split('/');
            if (parts.size() == 2) {
                QStringList num = parts[0].split('*', Qt::SkipEmptyParts);
                QStringList den = parts[1].split('*', Qt::SkipEmptyParts);
                return { num, den };
            }
        }
        return { QStringList(s), QStringList() };
        };

    auto fromParts = parseUnitExpr(fromUnit);
    auto toParts = parseUnitExpr(toUnit);

    auto computeFactor = [&](const QStringList& units, double& factor, bool& nonLinear) -> bool {
        factor = 1.0;
        nonLinear = false;
        for (const QString& unit : units) {
            if (!MAP.contains(unit)) return false;
            const ResolvedUnit& ru = MAP[unit];
            if (ru.isNonLinear) nonLinear = true;
            factor *= ru.toBase;
        }
        return true;
        };

    double fromNumFactor = 1.0, fromDenFactor = 1.0;
    double toNumFactor = 1.0, toDenFactor = 1.0;
    bool fromNonLin = false, toNonLin = false;

    if (!computeFactor(fromParts.first, fromNumFactor, fromNonLin) ||
        !computeFactor(fromParts.second, fromDenFactor, fromNonLin))
        return {};
    if (!computeFactor(toParts.first, toNumFactor, toNonLin) ||
        !computeFactor(toParts.second, toDenFactor, toNonLin))
        return {};

    // Non‑linear compounds are not allowed (temperature)
    if (fromNonLin || toNonLin)
        return {};

    double fromFactor = fromNumFactor / fromDenFactor;
    double toFactor = toNumFactor / toDenFactor;
    double result = val * fromFactor / toFactor;

    double factor = fromFactor / toFactor;
    QString formula = QString("1 %1 = %2 %3").arg(fromUnit).arg(factor).arg(toUnit);
    if (fromUnit == "mach") return { QString("%1 %2 = %3 %4").arg(fromUnit).arg(val).arg(result).arg(toUnit), ResultType::conv, formula };
    else if (toUnit == "mach") return { QString("%1 %2 = %3 %4").arg(val).arg(fromUnit).arg(toUnit).arg(result), ResultType::conv,formula };
    return {
        QString("%1 %2 = %3 %4").arg(val).arg(fromUnit).arg(BigNum::fmt(result)).arg(toUnit),
        ResultType::conv,
        formula
    };
}