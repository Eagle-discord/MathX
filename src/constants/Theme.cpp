#include "Theme.h"
#include "../settings/Settings.h"   // adjust path to match your tree
#include <QColor>

namespace Theme {

    QString accentColor() {
        const QString saved = Settings::instance().accentColor();
        return saved.isEmpty() ? QStringLiteral("#00e87a") : saved;
    }

    QString fontFamily() {
        const QString saved = Settings::instance().fontFamily();
        if (!saved.isEmpty() && QFontDatabase::families().contains(saved))
            return saved;
        for (const QString& f : { "JetBrains Mono", "Consolas", "Courier New" })
            if (QFontDatabase::families().contains(f)) return f;
        return QStringLiteral("Courier New");
    }

    int fontSize() {

        return 10;
    }

    QString dimAccent() {
        QColor c(accentColor());
        if (!c.isValid()) return QStringLiteral("#00b860");
        return QColor::fromHsvF(
            c.hsvHueF(),
            c.hsvSaturationF(),
            c.valueF() * 0.78f
        ).name();
    }

} // namespace Theme