// QArchiver / 小Q解压
// Security preflight for extraction: path traversal, unsafe symlinks and free
// disk space.
// 解压安全预检：路径穿越、危险符号链接与可用磁盘空间。

#include "core/ExtractionPreflight.h"

#include <QCoreApplication>
#include <QLocale>

#include "utils/FileUtils.h"

namespace ExtractionPreflight {

namespace {

// User-visible messages keep the "ArchiveManager" translation context so the
// existing catalogs (translations/*.ts) stay valid after the split.
// 用户可见消息沿用 "ArchiveManager" 翻译上下文，使拆分后现有翻译目录
// （translations/*.ts）继续有效。
QString tr(const char *text)
{
    return QCoreApplication::translate("ArchiveManager", text);
}

Result fail(ArchiveErrorCode code, const QString &message)
{
    Result result;
    result.ok = false;
    result.errorCode = code;
    result.errorMessage = message;
    return result;
}

} // namespace

Result check(const ExtractOptions &options,
             const QList<ArchiveEntry> &entries,
             const SymlinkDecisionCallback &symlinkDecisionCallback)
{
    if (options.destDir.trimmed().isEmpty()) {
        return fail(ArchiveErrorCode::Unknown,
                    tr("No destination directory selected"));
    }

    const auto isSelected = [&options](const QString &entryPath) {
        if (options.selectedFiles.isEmpty()) {
            return true;
        }
        const QString normalized = FileUtils::normalizeEntryPath(entryPath);
        for (const QString &selected : options.selectedFiles) {
            const QString selectedPath = FileUtils::normalizeEntryPath(selected);
            if (selectedPath.isEmpty()) {
                continue;
            }
            if (normalized == selectedPath
                || normalized.startsWith(selectedPath + QLatin1Char('/'))) {
                return true;
            }
        }
        return false;
    };
    // A symlink must also be inspected when the user selected a child that
    // would be reached through it.
    // 用户选中符号链接下的子项时，也必须检查该符号链接本身。
    const auto isSelectedOrAncestor = [&options, &isSelected](const QString &entryPath) {
        if (isSelected(entryPath)) {
            return true;
        }
        const QString normalized = FileUtils::normalizeEntryPath(entryPath);
        for (const QString &selected : options.selectedFiles) {
            const QString selectedPath = FileUtils::normalizeEntryPath(selected);
            if (!selectedPath.isEmpty()
                && selectedPath.startsWith(normalized + QLatin1Char('/'))) {
                return true;
            }
        }
        return false;
    };

    if (!options.selectedFiles.isEmpty()) {
        for (const QString &path : options.selectedFiles) {
            if (FileUtils::isPathTraversal(path, options.destDir)) {
                return fail(ArchiveErrorCode::PathTraversalBlocked,
                            tr("Blocked unsafe path: %1").arg(path));
            }
        }
    } else {
        for (const ArchiveEntry &entry : entries) {
            if (FileUtils::isPathTraversal(entry.path, options.destDir)) {
                return fail(ArchiveErrorCode::PathTraversalBlocked,
                            tr("Blocked unsafe path: %1").arg(entry.path));
            }
        }
    }

    Result result;

    // Pre-extraction symlink preflight.  The CLI tools must never be allowed
    // to create an escaping link and then write through it.
    // 解压前符号链接预检。绝不能让命令行工具创建逃逸链接后再通过它写入。
    if (!options.symlinkPreflightDone) {
        struct UnsafeLink {
            QString path;
            QString normalized;
            QString target;
        };
        QList<UnsafeLink> unsafe;
        for (const ArchiveEntry &entry : entries) {
            if (!entry.isSymLink || !isSelectedOrAncestor(entry.path)) {
                continue;
            }
            if (!FileUtils::isSafeSymlinkTarget(entry.path, entry.linkTarget)) {
                unsafe.append({entry.path,
                               FileUtils::normalizeEntryPath(entry.path),
                               entry.linkTarget});
            }
        }

        if (!unsafe.isEmpty()) {
            if (!symlinkDecisionCallback) {
                return fail(
                    ArchiveErrorCode::PathTraversalBlocked,
                    tr("Blocked unsafe symbolic link: %1").arg(unsafe.first().path));
            }

            bool skipAll = false;
            for (const UnsafeLink &link : unsafe) {
                SymlinkDecision decision = SymlinkDecision::Abort;
                if (!skipAll) {
                    decision = symlinkDecisionCallback(link.path, link.target);
                }
                if (decision == SymlinkDecision::Abort) {
                    return fail(
                        ArchiveErrorCode::Cancelled,
                        tr("Extraction cancelled because of an unsafe symbolic link: %1")
                            .arg(link.path));
                }
                result.skippedSymlinks.insert(link.normalized);
                if (decision == SymlinkDecision::SkipAll) {
                    skipAll = true;
                }
            }
        }
    } else {
        result.skippedSymlinks = options.skippedSymlinks;
    }

    qint64 required = 0;
    for (const ArchiveEntry &entry : entries) {
        if (entry.isDir || !isSelected(entry.path)) {
            continue;
        }
        if (!FileUtils::addSizeChecked(required, entry.size)) {
            return fail(
                ArchiveErrorCode::InsufficientDiskSpace,
                tr("Archive size is abnormal or too large to extract safely"));
        }
    }

    const qint64 available = FileUtils::estimateFreeSpace(options.destDir);
    constexpr qint64 kMargin = 1024 * 1024;
    if (required > 0 && available >= 0
        && (available < required || available - required < kMargin)) {
        return fail(ArchiveErrorCode::InsufficientDiskSpace,
                    tr("Insufficient disk space: %1 required, %2 available")
                        .arg(QLocale().formattedDataSize(required),
                             QLocale().formattedDataSize(available)));
    }

    return result;
}

} // namespace ExtractionPreflight
