#ifndef QARCHIVER_UI_ICONS_ICONUTILS_H
#define QARCHIVER_UI_ICONS_ICONUTILS_H

// QArchiver / 小Q解压
// Central access point for the embedded icon sets.
// 内嵌图标集的统一访问入口。

#include <QIcon>
#include <QPixmap>
#include <QSize>
#include <QString>

// Every icon lookup and every resource prefix lives here, so reorganizing the
// asset directories only touches this file instead of the whole UI layer.
// 所有图标查找与资源前缀都集中在此；资源目录重组时只需修改本文件，
// 而不用改整个界面层。
namespace Icons {

// Resource roots of the embedded icon sets and theme glyphs.
// 内嵌图标集与主题字形的资源根目录。
inline const char *aeroRoot() { return ":/icons/classic/"; }
inline const char *modernRoot() { return ":/icons/modern/"; }
inline const char *fileTypeRoot() { return ":/icons/file-types/"; }
inline const char *aeroGlyphRoot() { return ":/themes/aero/"; }
inline const char *modernGlyphRoot() { return ":/themes/modern/"; }

// Classic theme icon by file name, e.g. Icons::aero("Open.png").
// 按文件名取经典主题图标，例如 Icons::aero("Open.png")。
QIcon aero(const QString &fileName);

// Modern theme icon by file name, e.g. Icons::modern("extract.png").
// 按文件名取现代主题图标，例如 Icons::modern("extract.png")。
QIcon modern(const QString &fileName);

// File-type icon by file name, e.g. Icons::fileType("file-pdf.png").
// 按文件名取文件类型图标，例如 Icons::fileType("file-pdf.png")。
QIcon fileType(const QString &fileName);

// Glyph from the active theme's light/ or dark/ directory.  Both themes ship
// the same names in "light/" (for light backgrounds) and "dark/" (for dark
// backgrounds), so a theme change never requires renaming assets.
// 从当前主题的 light/ 或 dark/ 目录取字形。两套主题在 "light/"（浅色背景用）
// 与 "dark/"（深色背景用）下使用相同名称，因此切换主题无需重命名资源。
QIcon themed(const QString &baseName);

// Same as themed() but as a pixmap (used by custom paintEvent code).
// 同 themed()，但返回 pixmap（供自定义 paintEvent 使用）。
QPixmap themedPixmap(const QString &baseName);

// Application logo at the requested pixel size, scaled smoothly.
// 按请求的像素尺寸平滑缩放的应用图标。
QPixmap appLogo(int size);
QPixmap appLogo(const QSize &size);

} // namespace Icons

#endif // QARCHIVER_UI_ICONS_ICONUTILS_H
