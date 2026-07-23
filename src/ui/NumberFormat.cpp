#include "NumberFormat.h"

#include <QLocale>
#include <QRegularExpression>
#include "../settings/Settings.h"

namespace {

// Below this, grouping is clutter: four-digit numbers read fine unaided and
// most style guides leave them alone.
constexpr int kMinDigits = 5;

// Past this, three-digit grouping inflates the string by a third - painful when
// a result is already being truncated for display. Long digit strings are
// conventionally read in blocks of five instead.
constexpr int kLongRun = 200;
constexpr int kLongBlock = 5;


// -- Conway-Wechsler systematic -illion naming --------------------------------
// The n-th "-illion" is 10^(3n+3) on the short scale. Rather than a lookup table
// that runs out, this COMPOSES names from Latin roots, so it is total: there is
// no magnitude it refuses. Name length grows like log(log(x)) - a googolplex has
// a name under 800 characters.
//
// Note this is genuine Conway-Wechsler, which differs from the popular tables at
// three points: 15 is quinquadecillion (not quindecillion), 16 is sedecillion
// (not sexdecillion), 19 is novendecillion (not novemdecillion). Conway is the
// systematic form; the common ones are historical irregularities.
struct Root { const char* text; const char* flags; };

const char* kSmall[] = { "nil", "mi", "bi", "tri", "quadri",
                         "quinti", "sexti", "septi", "octi", "noni" };
const Root kUnits[] = { {"",""}, {"un",""}, {"duo",""}, {"tre",""}, {"quattuor",""},
                        {"quinqua",""}, {"se",""}, {"septe",""}, {"octo",""}, {"nove",""} };
const Root kTens[] = { {"",""}, {"deci","n"}, {"viginti","ms"}, {"triginta","ns"},
                       {"quadraginta","ns"}, {"quinquaginta","ns"}, {"sexaginta","n"},
                       {"septuaginta","n"}, {"octoginta","mx"}, {"nonaginta",""} };
const Root kHundreds[] = { {"",""}, {"centi","nx"}, {"ducenti","n"}, {"trecenti","ns"},
                           {"quadringenti","ns"}, {"quingenti","ns"}, {"sescenti","n"},
                           {"septingenti","n"}, {"octingenti","mx"}, {"nongenti",""} };

bool hasFlag(const char* flags, char c) {
    for (const char* p = flags; *p; ++p) if (*p == c) return true;
    return false;
}

// Latin root for one base-1000 chunk (0..999), before the -illion ending.
QString latinChunk(int c) {
    if (c == 0) return QStringLiteral("nil");
    if (c < 10)  return QString::fromLatin1(kSmall[c]);

    const int u = c % 10, t = (c / 10) % 10, h = c / 100;
    // Assimilation happens between the unit prefix and whatever root FOLLOWS it
    // (the tens root if there is one, otherwise the hundreds root).
    const Root& next = t ? kTens[t] : kHundreds[h];

    QString s;
    if (u) {
        s = QString::fromLatin1(kUnits[u].text);
        if (u == 3) {                                  // tre -> tres
            if (hasFlag(next.flags, 's') || hasFlag(next.flags, 'x')) s += QLatin1Char('s');
        }
        else if (u == 6) {                             // se -> ses / sex
            if (hasFlag(next.flags, 's'))      s += QLatin1Char('s');
            else if (hasFlag(next.flags, 'x')) s += QLatin1Char('x');
        }
        else if (u == 7 || u == 9) {                   // septe/nove -> ...n / ...m
            if (hasFlag(next.flags, 'n'))      s += QLatin1Char('n');
            else if (hasFlag(next.flags, 'm')) s += QLatin1Char('m');
        }
    }
    if (t) s += QString::fromLatin1(kTens[t].text);
    if (h) s += QString::fromLatin1(kHundreds[h].text);
    return s;
}

QString stripFinalVowel(QString s) {
    if (!s.isEmpty() && QStringLiteral("aeiou").contains(s.back())) s.chop(1);
    return s;
}

// -- English words for small whole numbers ------------------------------------
// "fifty", "one thousand two hundred thirty-four". Only up to (but not
// including) a million: past that the abbreviated illion reading is far more
// readable than a paragraph of words.
const char* kOnes[] = { "zero","one","two","three","four","five","six","seven",
    "eight","nine","ten","eleven","twelve","thirteen","fourteen","fifteen",
    "sixteen","seventeen","eighteen","nineteen" };
const char* kTensWord[10] = { "","","twenty","thirty","forty","fifty","sixty",
    "seventy","eighty","ninety" };

QString under1000(int n) {                 // 1..999
    if (n <= 0 || n > 999) return {};       // guard the fixed lookup tables
    QString s;
    if (n >= 100) { s += QLatin1String(kOnes[n / 100]) + QStringLiteral(" hundred"); n %= 100; if (n) s += QLatin1Char(' '); }
    if (n >= 20)  { s += QLatin1String(kTensWord[n / 10]); if (n % 10) { s += QLatin1Char('-'); s += QLatin1String(kOnes[n % 10]); } }
    else if (n > 0) s += QLatin1String(kOnes[n]);
    return s;
}

QString spellWhole(long long n) {          // 0 .. 999,999
    if (n < 0 || n > 999999) return {};     // callers must stay in range; guard anyway
    if (n == 0) return QStringLiteral("zero");
    QString s;
    if (n >= 1000) { s += under1000(int(n / 1000)) + QStringLiteral(" thousand"); n %= 1000; if (n) s += QLatin1Char(' '); }
    if (n) s += under1000(int(n));
    return s;
}

// Name of the n-th -illion. For n >= 1000 the index is written in base 1000 and
// the chunks are chained with "illi", which is what makes 1000 "millinillion"
// (mi + illi + nil + illion) rather than requiring a new word.
QString illionName(quint64 n) {
    if (n == 0) return {};
    QList<int> chunks;                       // least significant first
    for (quint64 m = n; m; m /= 1000) chunks << int(m % 1000);

    QString out;
    for (int i = chunks.size() - 1; i >= 0; --i)
        out += stripFinalVowel(latinChunk(chunks[i]))
        + (i ? QStringLiteral("illi") : QStringLiteral("illion"));
    return out;
}

QString groupRun(const QString& digits)
{
    const int n = digits.size();
    if (n < kMinDigits) return digits;

    const bool longRun = (n >= kLongRun);
    const int  block = longRun ? kLongBlock : 3;

    // Long runs use a plain space rather than the locale separator: a comma
    // every five digits reads as punctuation, not as grouping.
    const QString sep = longRun ? QStringLiteral(" ")
        : QLocale::system().groupSeparator();

    QString out;
    out.reserve(n + n / block + 1);

    // Group from the RIGHT. These are magnitudes, so the least significant digit
    // anchors the blocks; grouping from the left would silently change what the
    // number looks like it is.
    const int lead = n % block;
    int i = 0;
    if (lead > 0) { out += digits.left(lead); i = lead; }
    for (; i < n; i += block) {
        if (!out.isEmpty()) out += sep;
        out += digits.mid(i, block);
    }
    return out;
}

} // namespace

QString NumberFormat::groupNumbers(const QString& text)
{
    // Off by default. Read at CALL time rather than cached, so every render path
    // (result, scrub preview, hover, clipboard) picks the current value up on its
    // next repaint - which is most of what "live" needs.
    if (!Settings::instance().groupDigits()) return text;

    // Only runs that are clearly a whole number get touched:
    //   (?<![\w.])  not preceded by a word char or a decimal point - so
    //               identifiers ("x1234567") and fractional parts ("0.1234567")
    //               are left alone
    //   (?![\w])    not running straight into letters or more digits
    static const QRegularExpression run(R"((?<![\w.])(\d{5,})(?![\w]))");

    auto it = run.globalMatch(text);
    if (!it.hasNext()) return text;      // overwhelmingly the common case

    QString out;
    out.reserve(text.size() + text.size() / 3);
    int last = 0;
    while (it.hasNext()) {
        const auto m = it.next();
        out += QStringView{ text }.mid(last, m.capturedStart() - last);
        out += groupRun(m.captured(1));
        last = m.capturedEnd();
    }
    out += QStringView{ text }.mid(last);
    return out;
}

QString NumberFormat::scaleWord(const QString& text)
{
    constexpr int kMinDigitsForWord = 7;   // millions and up

    const QString t = text.trimmed();

    // Accept BOTH a bare integer and scientific notation. The scientific case is
    // what was silently failing: once fmt() collapses a big result to "1.55e+25"
    // the bare-integer test rejected it, so the reading vanished right where the
    // display switched to scientific (~10^21, just past quintillion). Normalise
    // both to a leading-digit string plus a total digit count, and the rest of
    // the function never has to know which form it started as.
    static const QRegularExpression bare(R"(^-?\d+$)");
    static const QRegularExpression sci(
        R"(^-?(\d+)(?:\.(\d+))?[eE]([+-]?\d+)$)");

    const bool neg = t.startsWith(QLatin1Char('-'));

    QString sig;            // significant (leading) digits, no point, no sign
    long long totalDigits;  // number of digits in the whole value

    if (bare.match(t).hasMatch()) {
        sig = neg ? t.mid(1) : t;
        totalDigits = sig.size();
    }
    else if (const auto m = sci.match(t); m.hasMatch()) {
        const QString ip = m.captured(1);          // digits before the point
        const QString fp = m.captured(2);          // digits after the point
        const long long exp = m.captured(3).toLongLong();
        sig = ip + fp;                             // "1" + "55" -> "155"
        // value = ip.fp x 10^exp, so its magnitude is ip.size() + exp digits.
        totalDigits = static_cast<long long>(ip.size()) + exp;
    }
    else {
        return {};
    }

    if (totalDigits < 1) return {};

    if (totalDigits < kMinDigitsForWord) {
        // Small enough to spell out in full. Reconstruct EXACTLY totalDigits
        // digits: take the leading significant digits (truncating any extra a
        // scientific mantissa carried) and right-pad with the value's trailing
        // zeros. This is what bounds n < 1,000,000 - without the truncation, a
        // preview value like 1.234567890123456e+4 kept its full 16-digit mantissa
        // and reconstructed to a 16-digit n, overrunning the speller's tables.
        const QString whole = sig.left(int(totalDigits))
            .leftJustified(int(totalDigits), QLatin1Char('0'));
        const long long n = whole.toLongLong();
        if (n < 20) return {};   // "5" vs "five" - no shorter, skip
        return (neg ? QStringLiteral("-") : QString()) + spellWhole(n);
    }

    long long group = (totalDigits - 1) / 3;
    const long long lead = totalDigits - group * 3;

    // Leading `lead` digits then four more for rounding, padding with the value's
    // trailing zeros when the significant digits run out (a scientific mantissa
    // carries far fewer digits than its magnitude).
    const QString padded = sig.leftJustified(int(lead + 4), QLatin1Char('0'));
    bool ok = false;
    double v = QStringLiteral("%1.%2")
        .arg(padded.left(int(lead)), padded.mid(int(lead), 4)).toDouble(&ok);
    if (!ok) return {};

    // Rounding can push a value up into the next scale: 999,999,999 is better
    // read as "1 billion" than "1000 million".
    if (v >= 999.5) { v /= 1000.0; ++group; }

    // Keep three significant figures without ever falling into exponent form.
    const int decimals = (v < 10.0) ? 2 : (v < 100.0 ? 1 : 0);
    QString num = QString::number(v, 'f', decimals);
    if (num.contains(QLatin1Char('.'))) {
        while (num.endsWith(QLatin1Char('0'))) num.chop(1);
        if (num.endsWith(QLatin1Char('.'))) num.chop(1);
    }
    return QStringLiteral("%1%2 %3")               // group g sits at (g-1)-illion
        .arg(neg ? QStringLiteral("-") : QString(), num,
            illionName(quint64(group) - 1));
}
