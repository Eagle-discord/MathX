#pragma once
#include <QString>
#include <cmath>

// -- Duration formatting -------------------------------------------------------
// One formatter, so every timer in the app agrees on units and precision.
//
// This exists because "Calculated in: %1 ms" was written out longhand in five
// places (MainWindow::appendTiming and four branches of
// OutputArea::addResultLine/buildResultHtml). Five copies is five chances to
// drift -- and they already had: the copy-history buffer and the on-screen
// label could print the same number with different precision.
//
// Scale rules, chosen so the number on screen is always 1-4 significant digits
// and never a wall of noise:
//
//     0.42        -> "0.42 ms"     sub-millisecond: 2 dp, the interesting range
//     6.345       -> "6.35 ms"     single-digit ms: 2 dp
//     43.16       -> "43.2 ms"     tens of ms: 1 dp
//     847.3       -> "847 ms"      hundreds of ms: whole numbers
//     1000        -> "1 second"    exactly one: singular, no decimal
//     6345        -> "6.35 seconds"
//     65000       -> "1 min 5 s"
//     3725000     -> "1 h 2 min 5 s"
//
// Note the singular "1 second" case: the request that prompted this was
// literally "if the time taken is 1000 ms, change it to 1 second", and a bare
// "1.00 seconds" would have missed the point.

namespace Duration {

    // Format a millisecond count for display. Never returns a unit suffix on
    // its own -- callers get a complete, human-readable phrase.
    inline QString fmt(double ms) {
        if (!std::isfinite(ms) || ms < 0.0) return QStringLiteral("-");

        // Sub-second: stay in milliseconds, scale precision to magnitude.
        // The < 999.5 bound (not < 1000) matters: 999.9 ms rounds to "1000" at
        // 0 dp, and "1000 ms" next to a "1 second" case reads as a bug.
        if (ms < 999.5) {
            if (ms < 10.0)  return QString::number(ms, 'f', 2) + QStringLiteral(" ms");
            if (ms < 100.0) return QString::number(ms, 'f', 1) + QStringLiteral(" ms");
            return QString::number(ms, 'f', 0) + QStringLiteral(" ms");
        }

        const double secs = ms / 1000.0;

        // Under a minute: seconds, with the exact-1 case spelled singular.
        // "Exact" is generous on purpose -- anything that would PRINT as 1.00
        // says "1 second", so 999.6 ms and 1000.4 ms don't render as
        // "1.00 seconds" right next to a bare "1 second".
        if (secs < 60.0) {
            if (secs < 10.0) {
                if (QString::number(secs, 'f', 2) == QLatin1String("1.00"))
                    return QStringLiteral("1 second");
                return QString::number(secs, 'f', 2) + QStringLiteral(" seconds");
            }
            return QString::number(secs, 'f', 1) + QStringLiteral(" seconds");
        }

        // A minute or more: break it up. A batch that takes this long is worth
        // reading at a glance, not decoding from "187493.2 ms".
        const qint64 total = static_cast<qint64>(secs + 0.5);
        const qint64 h = total / 3600;
        const qint64 m = (total % 3600) / 60;
        const qint64 s = total % 60;

        if (h > 0)
            return QStringLiteral("%1 h %2 min %3 s").arg(h).arg(m).arg(s);
        return QStringLiteral("%1 min %2 s").arg(m).arg(s);
    }

    // "Calculated in: 6.35 ms" -- the per-result line. Single source of truth
    // for the label text, so the on-screen and copy-history forms can't drift.
    inline QString calculatedIn(double ms) {
        return QStringLiteral("Calculated in: ") + fmt(ms);
    }

} // namespace Duration