#ifndef QARCHIVER_UTILS_FILEASSOCIATIONS_H
#define QARCHIVER_UTILS_FILEASSOCIATIONS_H

// QArchiver / 小Q解压
// Per-user archive file associations (Windows registry / Linux mimeapps.list).
// 当前用户的压缩包格式关联（Windows 注册表 / Linux mimeapps.list）。

#include <QString>
#include <QStringList>

// Small platform wrapper used by the installer, the settings dialog and the
// command line.  Only the current user is modified; system-wide defaults are
// managed by the operating system.
// 供安装器、设置对话框与命令行使用的小型平台封装。只修改当前用户；
// 系统级默认值由操作系统管理。
namespace FileAssociations {

// Identity of the installed desktop entry.  The freedesktop desktop-entry ID is
// the file name without the ".desktop" suffix; it is also what
// QGuiApplication::setDesktopFileName() and xdg-mime defaults use, so it must
// match the installed file name.  The packaging rules keep that name equal to
// the Debian package name (yincolor.qarchiver).
// 已安装桌面入口的标识。freedesktop 桌面入口 ID 是去掉 ".desktop" 后缀的文件名，
// 同时也是 QGuiApplication::setDesktopFileName() 与 xdg-mime 默认关联使用的值，
// 因此必须与安装的文件名一致；打包规则保证它与 Debian 包名相同。
QString desktopEntryId();

// Installed desktop entry file name, i.e. desktopEntryId() + ".desktop".
// 安装的桌面入口文件名，即 desktopEntryId() + ".desktop"。
QString desktopEntryFileName();

// Archive extensions handled by QArchiver, with a leading dot.
// QArchiver 支持的压缩包扩展名（含前导点）。
QStringList extensions();

// Human-readable description for an extension (e.g. "ZIP 压缩包").
// 扩展名的可读描述（例如“ZIP 压缩包”）。
QString descriptionFor(const QString &extension);

// Whether this platform has a working association backend.
// 当前平台是否有可用的关联实现。
bool isSupported();

// Register/unregister the given extensions for the current user.  Each call
// updates the per-user association entry and the "Open with" list.
// 为当前用户注册/取消注册指定扩展名；同时更新“打开方式”列表。
bool registerFormats(const QStringList &extensions, QString *error = nullptr);
bool unregisterFormats(const QStringList &extensions, QString *error = nullptr);

// Whether our ProgID is registered for the extension.
// 扩展名是否已注册到 QArchiver 的 ProgID。
bool isFormatRegistered(const QString &extension);

// Open the platform's default-apps page when the dialog needs the user to
// confirm a default association.  Returns false when no such page exists.
// 打开平台的默认应用设置页（当系统要求用户确认默认关联时）。
bool openDefaultAppsSettings(QString *error = nullptr);

} // namespace FileAssociations

#endif // QARCHIVER_UTILS_FILEASSOCIATIONS_H
