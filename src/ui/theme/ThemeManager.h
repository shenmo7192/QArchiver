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
    // Query the platform color-scheme hint without blocking startup (no
    // synchronous DBus call).  Returns "dark" when the system prefers a dark
    // appearance, otherwise "light"; Qt 6.4 falls back to palette lightness.
    // 查询平台颜色方案提示，且不阻塞启动（不同步调用 DBus）。
    // 系统偏好深色时返回 "dark"，否则返回 "light"；Qt 6.4 使用调色板明度回退。
    static QString systemColorScheme();

    // Resolve a configured theme value ("system" / "light" / "dark") to the
    // effective scheme: either "light" or "dark".
    // 将配置主题值解析为实际方案："light" 或 "dark"。
    static QString effectiveTheme(const QString &configuredTheme);

    // Read AppSettings::theme() and apply the resolved palette to the app.
    // 读取 AppSettings::theme() 并把解析后的调色板应用到应用。
    static void applyTheme(QApplication &app);

    // Whether the modern layout ("modernUi") is active.  These two queries are
    // the only supported way to inspect the applied theme; widgets and models
    // must not read the "modernUi"/"aeroDark" properties directly.
    // 当前是否为现代布局（"modernUi"）。这两个查询是检查已应用主题的唯一
    // 受支持方式；控件与模型不得直接读取 "modernUi"/"aeroDark" 属性。
    static bool isModernLayout();

    // Whether the dark color scheme is active. / 当前是否为深色配色。
    static bool isDarkTheme();
};

#endif // THEMEMANAGER_H
