// QArchiver / 小Q解压
// ThemeManager implementation using palettes and the FreeDesktop color scheme.
// ThemeManager 实现，使用调色板与 FreeDesktop 配色方案。

#include "ThemeManager.h"

#include <QApplication>
#include <QColor>
#ifdef QARCHIVER_HAS_DBUS
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#endif
#include <QGuiApplication>
#include <QPalette>
#include <QStyleHints>

#include "core/AppSettings.h"
#include "utils/Logger.h"

namespace {

QPalette buildPalette(bool dark)
{
    QPalette p;

    if (dark) {
        const QColor window(53, 53, 53);
        const QColor windowText(220, 220, 220);
        const QColor base(30, 30, 30);
        const QColor alternateBase(45, 45, 45);
        const QColor toolTipBase(60, 60, 60);
        const QColor toolTipText(220, 220, 220);
        const QColor text(220, 220, 220);
        const QColor button(53, 53, 53);
        const QColor buttonText(220, 220, 220);
        const QColor brightText(255, 255, 255);
        const QColor link(42, 130, 218);
        const QColor highlight(42, 130, 218);
        const QColor highlightedText(255, 255, 255);
        const QColor placeholder(127, 127, 127);

        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, windowText);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, alternateBase);
        p.setColor(QPalette::ToolTipBase, toolTipBase);
        p.setColor(QPalette::ToolTipText, toolTipText);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, buttonText);
        p.setColor(QPalette::BrightText, brightText);
        p.setColor(QPalette::Link, link);
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, highlightedText);
        p.setColor(QPalette::PlaceholderText, placeholder);

        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    } else {
        const QColor window(239, 240, 241);
        const QColor windowText(35, 38, 41);
        const QColor base(255, 255, 255);
        const QColor alternateBase(247, 247, 247);
        const QColor toolTipBase(255, 255, 220);
        const QColor toolTipText(35, 38, 41);
        const QColor text(35, 38, 41);
        const QColor button(239, 240, 241);
        const QColor buttonText(35, 38, 41);
        const QColor brightText(255, 255, 255);
        const QColor link(41, 100, 200);
        const QColor highlight(42, 130, 218);
        const QColor highlightedText(255, 255, 255);
        const QColor placeholder(128, 128, 128);

        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, windowText);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, alternateBase);
        p.setColor(QPalette::ToolTipBase, toolTipBase);
        p.setColor(QPalette::ToolTipText, toolTipText);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, button);
        p.setColor(QPalette::ButtonText, buttonText);
        p.setColor(QPalette::BrightText, brightText);
        p.setColor(QPalette::Link, link);
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, highlightedText);
        p.setColor(QPalette::PlaceholderText, placeholder);

        p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(150, 150, 150));
        p.setColor(QPalette::Disabled, QPalette::Text, QColor(150, 150, 150));
        p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(150, 150, 150));
        p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(200, 200, 200));
        p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(150, 150, 150));
    }

    return p;
}

} // namespace

QString ThemeManager::systemColorScheme()
{
#ifdef QARCHIVER_HAS_DBUS
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        LOG_WARN_S("ThemeManager", "Session bus unavailable; defaulting to light theme");
        return QStringLiteral("light");
    }

    QDBusInterface iface(QStringLiteral("org.freedesktop.portal.Desktop"),
                         QStringLiteral("/org/freedesktop/portal/desktop"),
                         QStringLiteral("org.freedesktop.portal.Settings"),
                         bus);
    if (!iface.isValid()) {
        LOG_WARN_S("ThemeManager",
                   "org.freedesktop.portal.Settings unavailable; defaulting to light theme");
        return QStringLiteral("light");
    }

    const QDBusReply<QVariant> reply = iface.call(
        QStringLiteral("Read"),
        QStringLiteral("org.freedesktop.appearance"),
        QStringLiteral("color-scheme"));

    if (!reply.isValid()) {
        LOG_WARN_S("ThemeManager",
                   "color-scheme query failed; defaulting to light theme");
        return QStringLiteral("light");
    }

    // org.freedesktop.appearance color-scheme values:
    //   0 = no preference, 1 = prefer dark, 2 = prefer light.
    // The portal returns the value as a variant (QDBusVariant), so unwrap it
    // before converting to a number.
    // org.freedesktop.appearance color-scheme 取值：
    //   0 = 无偏好，1 = 偏好深色，2 = 偏好浅色。
    // 门户以 variant（QDBusVariant）返回该值，转换数字前需先解包。
    QVariant rawValue = reply.value();
    if (rawValue.canConvert<QDBusVariant>()) {
        rawValue = rawValue.value<QDBusVariant>().variant();
    }
    bool ok = false;
    const uint value = rawValue.toUInt(&ok);
    if (!ok) {
        LOG_WARN_S("ThemeManager",
                   "color-scheme value is not a number; defaulting to light theme");
        return QStringLiteral("light");
    }

    return (value == 1) ? QStringLiteral("dark") : QStringLiteral("light");
#else
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark
        ? QStringLiteral("dark")
        : QStringLiteral("light");
#endif
}

QString ThemeManager::effectiveTheme(const QString &configuredTheme)
{
    const QString t = configuredTheme.trimmed().toLower();
    if (t == QStringLiteral("dark")) {
        return QStringLiteral("dark");
    }
    if (t == QStringLiteral("light")) {
        return QStringLiteral("light");
    }
    // "system" (or any unknown value): follow the desktop color-scheme.
    // "system"（或任何未知值）：跟随桌面配色方案。
    return systemColorScheme();
}

void ThemeManager::applyTheme(QApplication &app)
{
    const QString theme = effectiveTheme(AppSettings::instance().theme());
    const bool dark = (theme == QStringLiteral("dark"));

    // Ask the platform style to adopt the requested color scheme first, then
    // install our explicit palette so every widget renders consistently.
    // 先让平台样式采用请求的配色方案，再设置显式调色板，
    // 确保所有控件渲染一致。
    if (auto *hints = QGuiApplication::styleHints()) {
        hints->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
    }
    app.setPalette(buildPalette(dark));

    LOG_INFO_S("ThemeManager", dark ? "Applied dark theme" : "Applied light theme");
}
