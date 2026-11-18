#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

// QArchiver / 小Q解压
// Resolves and applies light, dark or system themes.
// 解析并应用浅色、深色或系统主题。

#include <QString>

class QApplication;

// Central theme manager (design doc 4.6, Preferences -> Theme).
// Resolves the configured theme ("system" / "light" / "dark") into an
// effective light or dark appearance and applies it to the whole application.
// 中心主题管理器（设计文档 4.6，偏好设置 -> 主题）。
// 将配置值（"system" / "light" / "dark"）解析为实际浅色或深色外观，
// 并应用到整个应用。
class ThemeManager {
public:
    // Query the FreeDesktop portal setting org.freedesktop.appearance
    // color-scheme via Qt DBus. Returns "dark" when the system prefers a
    // dark appearance, otherwise "light" (including when the query fails).
    // 通过 Qt DBus 查询 FreeDesktop 门户的 org.freedesktop.appearance
    // color-scheme。系统偏好深色时返回 "dark"，否则返回 "light"（查询失败亦然）。
    static QString systemColorScheme();

    // Resolve a configured theme value ("system" / "light" / "dark") to the
    // effective scheme: either "light" or "dark".
    // 将配置主题值解析为实际方案："light" 或 "dark"。
    static QString effectiveTheme(const QString &configuredTheme);

    // Read AppSettings::theme() and apply the resolved palette to the app.
    // 读取 AppSettings::theme() 并把解析后的调色板应用到应用。
    static void applyTheme(QApplication &app);
};

#endif // THEMEMANAGER_H
