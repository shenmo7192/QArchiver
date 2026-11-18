#ifndef SEVENZIPUTILS_H
#define SEVENZIPUTILS_H

// QArchiver / 小Q解压
// Shared 7-Zip path, listfile and legacy-encoding helpers.
// 共用的 7-Zip 路径、列表文件与旧编码辅助函数。

#include <QByteArray>
#include <QString>
#include <QStringList>

class QTemporaryFile;

// Helpers shared by the 7-Zip based backends (7z/zip, tar, rar).
namespace SevenZipUtils {

// Resolve the bundled 7-Zip executable (or the QARCHIVER_7Z_PATH override).
// 解析内置 7-Zip 可执行文件（或 QARCHIVER_7Z_PATH 覆盖值）。
QString resolveSevenZip();

// Write @p entries to a UTF-8 or legacy-codepage listfile and append the
// matching -scs/-i@ switches to @p args.  A temporary file keeps the command
// line free of wildcard/leading-dash/@-listfile surprises.
// 将 @p entries 写入 UTF-8 或旧代码页列表文件，并向 @p args 追加
// 对应的 -scs/-i@ 开关。使用临时文件可避免通配符、前导短横线
// 和 @ 列表文件被命令行误解析。
bool appendListFileArgs(QTemporaryFile &file, const QStringList &entries,
                        const QString &encoding, QStringList &args);

// 7-Zip on Linux writes legacy-codepage member names as raw bytes, so the
// extracted tree can contain GBK/Big5/... filenames.  Rename them to UTF-8
// bottom-up (children first, then the directory itself).  Windows is a no-op:
// 7z.exe already creates Unicode filenames.
// Linux 上的 7-Zip 会以原始字节写出旧代码页成员名，因此解压目录中可能出现
// GBK/Big5 等文件名。这里自底向上（先子项、后目录本身）重命名为 UTF-8。
// Windows 上不做处理：7z.exe 已创建 Unicode 文件名。
void renameLegacyExtractedTree(const QByteArray &dirBytes, const QString &encoding);

} // namespace SevenZipUtils

#endif // SEVENZIPUTILS_H
