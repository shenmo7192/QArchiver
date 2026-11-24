// QArchiver / 小Q解压
// Central access point for the embedded icon sets.
// 内嵌图标集的统一访问入口。

#include "ui/icons/IconUtils.h"

#include "ui/theme/ThemeManager.h"

namespace {

constexpr const char *kPngSuffix = ".png";
constexpr const char *kLightDir = "light/";
constexpr const char *kDarkDir = "dark/";

} // namespace

namespace Icons {

QIcon aero(const QString &fileName)
{
    return QIcon(QLatin1String(aeroRoot()) + fileName);
}

QIcon modern(const QString &fileName)
{
    return QIcon(QLatin1String(modernRoot()) + fileName);
}

QIcon fileType(const QString &fileName)
{
    const bool modern = ThemeManager::isModernLayout();
    return QIcon(QLatin1String(fileTypeRoot(modern)) + fileName);
}

QIcon themed(const QString &baseName)
{
    const QString root = QLatin1String(ThemeManager::isModernLayout()
                                           ? modernGlyphRoot()
                                           : aeroGlyphRoot());
    const QString dir = QLatin1String(ThemeManager::isDarkTheme() ? kDarkDir
                                                                  : kLightDir);
    return QIcon(root + dir + baseName + QLatin1String(kPngSuffix));
}

QPixmap themedPixmap(const QString &baseName)
{
    return themed(baseName).pixmap(16, 16);
}

QPixmap appLogo(int size)
{
    return appLogo(QSize(size, size));
}

QPixmap appLogo(const QSize &size)
{
    return modern(QStringLiteral("app.png")).pixmap(size);
}

} // namespace Icons
