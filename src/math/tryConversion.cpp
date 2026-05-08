// ----------------------------------------------------------------------
//  tryConversion – universal unit conversion system
//
//  Architecture:
//    1. SI_BASES  — one entry per physical quantity's base unit.
//                   Prefixable bases get all 20 SI prefixes generated
//                   automatically (km, mm, nm, Mm, etc.)
//
//    2. MANUAL    — non-SI units with known fixed factors, plus
//                   aliases, plural forms, and common abbreviations.
//
//    3. NONLINEAR — temperature and other offset/reciprocal units
//                   that can't be represented as a simple factor.
//
//    4. buildUnitMap() — called once, merges all three sources into a
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


// ── Resolved unit ─────────────────────────────────────────────────────────────
struct ResolvedUnit {
    QString category;
    double  toBase = 0;
    std::function<double(double)> toBaseFn;
    std::function<double(double)> fromBaseFn;
    bool    isNonLinear = false;   // explicit flag for non‑linear units
    bool    valid = false;
};

// ── SI prefix table ───────────────────────────────────────────────────────────
struct SIPrefix { QString sym; double factor; };
static const QVector<SIPrefix> SI_PREFIXES = {
    {"Y",1e24},{"Z",1e21},{"E",1e18},{"P",1e15},{"T",1e12},{"G",1e9},
    {"M",1e6}, {"k",1e3}, {"h",1e2}, {"da",1e1},
    {"d",1e-1},{"c",1e-2},{"m",1e-3},{"u",1e-6},{"n",1e-9},
    {"p",1e-12},{"f",1e-15},{"a",1e-18},{"z",1e-21},{"y",1e-24},
};

// ── SI base definitions ───────────────────────────────────────────────────────
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
// ── Manual non-SI units ───────────────────────────────────────────────────────
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

    { "m/s",     "speed",  1.0,             {"meterpersecond","meterspersecond","mps"} },
    { "km/h",    "speed",  1.0 / 3.6,         {"kph","kmh"} },
    { "ft/s",    "speed",  0.3048,          {"fps"} },
    {"mph",     "speed",  0.44704,         {"mileperhour"}},
    {"kph",     "speed",  1.0 / 3.6,         {"km/h","kmh"}},
    {"knot",    "speed",  0.51444444,      {"knots","kn","kt"}},
    {"fps",     "speed",  0.3048,          {"ft/s"}},
    {"mach",    "speed",  343.0,           {}},
    // Frequency (base: Hz)
    {"rpm",     "frequency",1.0 / 60.0,      {}},
    {"rps",     "frequency",1.0,           {}},
    // Data (base: byte) — SI decimal
    {"kb",      "data",   1000.0,          {"kilobyte","kilobytes"}},
    {"mb",      "data",   1e6,             {"megabyte","megabytes"}},
    {"gb",      "data",   1e9,             {"gigabyte","gigabytes"}},
    {"tb",      "data",   1e12,            {"terabyte","terabytes"}},
    {"pb",      "data",   1e15,            {"petabyte","petabytes"}},
    {"eb",      "data",   1e18,            {"exabyte","exabytes"}},
    // Data — binary IEC
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

// ── Non-linear units ──────────────────────────────────────────────────────────
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
        {"celsius","degc"}},
    {"f",  "temperature",
        [](double v) { return (v - 32.0) * 5.0/9.0 + 273.15; }, // °F → K
        [](double v) { return (v - 273.15) * 9.0/5.0 + 32.0; }, // K → °F
        {"fahrenheit","degf"}},
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
        // L/100km — reciprocal of km/L
        {"l/100km","fueleco",
            [](double v) { return 100.0 / v; },
            [](double v) { return 100.0 / v; },
            {"lper100km","l100km"}},
};

// ── Build lookup map (once) ───────────────────────────────────────────────────
static QMap<QString, ResolvedUnit> buildUnitMap() {
    QMap<QString, ResolvedUnit> map;

    auto addLinear = [&](const QString& rawKey, const QString& cat, double toBase) {
        QString k = rawKey.toLower();
        if (!map.contains(k))
            map[k] = { cat, toBase, {}, {}, false, true };
        };

    auto addNonLinear = [&](const QString& rawKey, const QString& cat,
        std::function<double(double)> toFn,
        std::function<double(double)> fromFn) {
            QString k = rawKey.toLower();
            if (!map.contains(k))
                map[k] = { cat, 0, toFn, fromFn, true, true };
        };

    // 1. Non-linear units (temperature etc.) – highest priority
    for (const NonLinearUnit& u : NONLINEAR) {
        addNonLinear(u.key, u.category, u.toBase, u.fromBase);
        for (const QString& alias : u.aliases)
            addNonLinear(alias, u.category, u.toBase, u.fromBase);
    }

    // 2. Manual non-SI units – takes precedence over auto‑generated ones
    for (const ManualUnit& u : MANUAL) {
        addLinear(u.key, u.category, u.toBase);
        for (const QString& alias : u.aliases)
            addLinear(alias, u.category, u.toBase);
    }

    // 3. SI bases + prefixes – only add if not already present
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

    return map;
}
// ── tryConversion ─────────────────────────────────────────────────────────────
CalcResult MathEngine::tryConversion(const QString& expr) {
    static const QMap<QString, ResolvedUnit> MAP = buildUnitMap();

    // Improved regex: allows signs, decimals, exponent, and unit names with '/'
    static QRegularExpression re(
        R"(^\s*([+-]?\d*\.?\d+(?:[eE][+-]?\d+)?)\s+([A-Za-z0-9/]+)\s+to\s+([A-Za-z0-9/]+)\s*$)",
        QRegularExpression::CaseInsensitiveOption);

    auto m = re.match(expr.trimmed());
    if (!m.hasMatch()) return {};

    double  val = m.captured(1).toDouble();
    QString from = m.captured(2).toLower();
    QString to = m.captured(3).toLower();

    if (!MAP.contains(from) || !MAP.contains(to)) return {};

    const ResolvedUnit& uFrom = MAP[from];
    const ResolvedUnit& uTo = MAP[to];
    if (uFrom.category != uTo.category) return {};



    double result = 0.0;
    bool nonLinear = uFrom.isNonLinear || uTo.isNonLinear;

    if (nonLinear) {
        double base = uFrom.toBaseFn ? uFrom.toBaseFn(val) : val * uFrom.toBase;
        result = uTo.fromBaseFn ? uTo.fromBaseFn(base) : base / uTo.toBase;
    }
    else {
        result = val * uFrom.toBase / uTo.toBase;
    }

    if (std::isnan(result) || std::isinf(result))
        return {};
    double factor = uFrom.toBase / uTo.toBase;
    QString formula = QString("1 %1 = %2 %3").arg(from).arg(factor).arg(to);
    return { QString("%1 %2 = %3 %4").arg(val).arg(from).arg(BigNum::fmt(result)).arg(to),
             "conv",
             formula };
    
}