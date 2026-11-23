#ifndef QARCHIVER_UTILS_MIMEAPPSLIST_H
#define QARCHIVER_UTILS_MIMEAPPSLIST_H

// QArchiver / 小Q解压
// Minimal reader/writer for the freedesktop mimeapps.list format.
// freedesktop mimeapps.list 格式的最小读写实现。

#include <QString>
#include <QStringList>

// QSettings' INI backend escapes group names and treats '/' in keys as a
// hierarchy separator, so it cannot be used for mimeapps.list.  This helper
// keeps the standard desktop-entry syntax intact:
//
//   [Default Applications]
//   application/zip=qarchiver.desktop;
//
// 不能使用 QSettings 的 INI 后端：它会转义组名并把键中的 '/' 当成层级分隔符。
// 本辅助模块保留标准桌面入口语法。
namespace MimeAppsList {

// Desktop IDs for mimeType in section, in file order.
// 指定节中 mimeType 对应的 desktop ID（按文件顺序）。
QStringList desktopIds(const QString &filePath, const QString &section,
                       const QString &mimeType);

// Whether a section maps mimeType to desktopId.
// 指定节是否把 mimeType 映射到 desktopId。
bool hasDesktopId(const QString &filePath, const QString &section,
                  const QString &mimeType, const QString &desktopId);

// Add or remove desktopId for mimeType in both "Default Applications" and
// "Added Associations", preserving unrelated sections and comments.
// 在 "Default Applications" 与 "Added Associations" 两节中添加或移除
// desktopId，同时保留无关的节与注释。
bool setAssociation(const QString &filePath, const QString &mimeType,
                    const QString &desktopId, bool add,
                    QString *error = nullptr);

} // namespace MimeAppsList

#endif // QARCHIVER_UTILS_MIMEAPPSLIST_H
