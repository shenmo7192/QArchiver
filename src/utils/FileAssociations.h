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
