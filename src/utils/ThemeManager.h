#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QString>

class QApplication;

// Central theme manager (design doc 4.6, Preferences -> Theme).
// Resolves the configured theme ("system" / "light" / "dark") into an
// effective light or dark appearance and applies it to the whole application.
class ThemeManager {
public:
    // Query the FreeDesktop portal setting org.freedesktop.appearance
    // color-scheme via Qt DBus. Returns "dark" when the system prefers a
    // dark appearance, otherwise "light" (including when the query fails).
    static QString systemColorScheme();

    // Resolve a configured theme value ("system" / "light" / "dark") to the
    // effective scheme: either "light" or "dark".
    static QString effectiveTheme(const QString &configuredTheme);

    // Read AppSettings::theme() and apply the resolved palette to the app.
    static void applyTheme(QApplication &app);
};

#endif // THEMEMANAGER_H
