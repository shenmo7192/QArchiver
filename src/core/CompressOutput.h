#ifndef QARCHIVER_CORE_COMPRESSOUTPUT_H
#define QARCHIVER_CORE_COMPRESSOUTPUT_H

// QArchiver / 小Q解压
// Compressed output staging: pattern matching, atomic promotion and cleanup.
// 压缩输出暂存：文件名匹配、原子提升与清理。

#include <QRegularExpression>
#include <QString>

#include "common/ArchiveConstants.h"

class QFileInfo;

// Compression writes into a temporary directory first and only replaces the
// target archive once the new output is complete.  These helpers implement the
// file-system part so ArchiveManager only tracks the session state.
// 压缩先写入临时目录，只有新输出完整后才替换目标压缩包。这些辅助函数实现
// 文件系统部分，ArchiveManager 只负责维护会话状态。
namespace CompressOutput {

// Pattern matching the target archive and any volumes produced next to it
// (e.g. archive.7z.001, archive.part1.rar).
// 匹配目标压缩包及其相邻分卷（例如 archive.7z.001、archive.part1.rar）。
QRegularExpression outputPattern(const QFileInfo &finalInfo,
                                 ArchiveFormat format);

// Whether the target archive (or one of its volumes) already exists.
// 目标压缩包（或其某个分卷）是否已存在。
bool outputExists(const QFileInfo &finalInfo, ArchiveFormat format);

// Move the produced files from @p tempPath into the directory of @p finalInfo,
// backing up and restoring an existing archive set on failure.
// 将 @p tempPath 中生成的文件移入 @p finalInfo 所在目录；失败时备份并恢复
// 已存在的压缩包集。
bool promote(const QFileInfo &finalInfo, ArchiveFormat format,
             const QString &tempPath, bool targetExisted);

// Remove every file produced for @p finalInfo (used when compression failed
// after the target was already replaced, or to clean a partial result).
// 删除为 @p finalInfo 生成的所有文件（压缩在目标已被替换后失败，或清理
// 不完整结果时使用）。
void removeOutput(const QFileInfo &finalInfo, ArchiveFormat format);

} // namespace CompressOutput

#endif // QARCHIVER_CORE_COMPRESSOUTPUT_H
