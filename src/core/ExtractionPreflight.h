#ifndef QARCHIVER_CORE_EXTRACTIONPREFLIGHT_H
#define QARCHIVER_CORE_EXTRACTIONPREFLIGHT_H

// QArchiver / 小Q解压
// Security preflight for extraction: path traversal, unsafe symlinks and free
// disk space.
// 解压安全预检：路径穿越、危险符号链接与可用磁盘空间。

#include <QList>
#include <QSet>
#include <QString>

#include <functional>

#include "common/ArchiveEntry.h"
#include "common/ArchiveError.h"
#include "common/ArchiveOptions.h"

namespace ExtractionPreflight {

// What to do with an unsafe symbolic link found in the archive.
// 发现压缩包内危险符号链接时的处理方式。
enum class SymlinkDecision { Abort, Skip, SkipAll };

// UI hook used during the preflight.  Called on the GUI thread; a null
// callback aborts the operation when an unsafe link is found.
// 预检期间使用的界面回调。在 GUI 线程调用；为空时遇到危险链接会中止操作。
using SymlinkDecisionCallback =
    std::function<SymlinkDecision(const QString &entryPath,
                                  const QString &target)>;

// Result of the preflight, including the links the user chose to skip.
// 预检结果，包含用户选择跳过的链接。
struct Result {
    bool ok = true;
    ArchiveErrorCode errorCode = ArchiveErrorCode::Unknown;
    QString errorMessage;
    QSet<QString> skippedSymlinks;
};

// Validate @p options against the archive listing in @p entries.
// 使用 @p entries 中的列表结果校验 @p options。
Result check(const ExtractOptions &options,
             const QList<ArchiveEntry> &entries,
             const SymlinkDecisionCallback &symlinkDecisionCallback);

} // namespace ExtractionPreflight

#endif // QARCHIVER_CORE_EXTRACTIONPREFLIGHT_H
