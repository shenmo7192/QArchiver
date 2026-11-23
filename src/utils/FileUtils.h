#ifndef FILEUTILS_H
#define FILEUTILS_H

// QArchiver / 小Q解压
// Filesystem helpers for extraction, safe paths and atomic moves.
// 用于解压、安全路径与原子移动的文件系统辅助函数。

#include <QString>
#include <QStringList>
#include <QList>

#include "core/ArchiveConstants.h"

struct ArchiveEntry;

// File/path utility functions / 文件与路径辅助函数
class FileUtils {
public:
    // Canonicalize a path (resolve symlinks, remove .. and .)
    // 规范化路径（解析符号链接，移除 .. 与 .）
    static QString canonicalPath(const QString &path);

    // Estimate free disk space (bytes) at the given path; -1 on failure
    // 估算指定路径的可用磁盘空间（字节）；失败返回 -1
    static qint64 estimateFreeSpace(const QString &path);

    // Count all files (recursively) under the given paths
    // 递归统计指定路径下的所有文件
    static int countFiles(const QStringList &paths);

    // Add @p value to @p total, rejecting negative values and qint64 overflow.
    // 将 @p value 累加到 @p total，拒绝负值与 qint64 溢出。
    static bool addSizeChecked(qint64 &total, qint64 value);

    // Estimate the total uncompressed size of an archive's entries.  The
    // returned value saturates at qint64 max on overflow.
    // 估算压缩包条目的解压后总大小；溢出时饱和到 qint64 最大值。
    static qint64 totalUnpackedSize(const QList<ArchiveEntry> &entries);

    // Checked variant used by extraction/drag preflight.  Returns false when
    // an entry size is negative or the sum exceeds qint64.
    // 解压/拖出预检使用的带检查版本；条目大小为负或总和溢出时返回 false。
    static bool totalUnpackedSizeChecked(const QList<ArchiveEntry> &entries,
                                         qint64 *total);

    // Whether a symlink stored at @p entryPath may point to @p target without
    // escaping the archive root.  Targets are interpreted relative to the
    // symlink's parent directory.
    // 位于 @p entryPath 的符号链接指向 @p target 时是否不会逃逸压缩包根目录。
    // 目标相对于符号链接所在目录解释。
    static bool isSafeSymlinkTarget(const QString &entryPath,
                                    const QString &target);

    // Inspect an extracted tree and return normalized paths of symlinks whose
    // final target escapes the tree (absolute targets, ".." escapes, or
    // existing symlink chains resolving outside).  This is the move-time
    // defence after pre-extraction validation.
    // 检查解压后的目录树，返回最终目标逃逸目录的符号链接规范化路径
    // （绝对目标、".." 逃逸或已存在的链接链解析到外部）。
    // 这是解压前预检之后的移动前复查防线。
    static QStringList unsafeExtractedSymlinks(const QString &srcDir);

    // Recursively remove a directory and all contents
    // 递归删除目录及其全部内容
    static bool removeDirectoryRecursively(const QString &path);

    // Atomically move (rename) a file; cross-filesystem fallback to copy+delete.
    // Symlinks are preserved as symlinks.
    // 原子移动（重命名）文件；跨文件系统时回退为复制 + 删除。
    // 符号链接仍以符号链接形式保留。
    static bool moveFileAtomically(const QString &src, const QString &dest);

    // Move everything inside srcDir into destDir.
    // Directories are recreated with mkpath instead of renamed (renaming a
    // directory would invalidate the source paths of all of its children).
    // When preservePaths is false, only the basename of each entry is kept.
    // The source directory is removed only when every entry was moved.
    // Returns the number of entries that could not be moved.
    // 将 srcDir 中的所有内容移动到 destDir。
    // 目录使用 mkpath 重建而不是重命名，否则会破坏其所有子项的源路径。
    // preservePaths 为 false 时仅保留每个条目的基本名称。
    // 仅当所有条目都移动成功后才删除源目录。
    // 返回无法移动的条目数量。
    static int moveExtractedContents(const QString &srcDir, const QString &destDir,
                                     OverwritePolicy overwrite,
                                     bool preservePaths = true);

    // Create a temporary extraction directory: destDir/.tmp_extract_<uuid>/
    // 创建临时解压目录：destDir/.tmp_extract_<uuid>/
    static QString createTempExtractDir(const QString &destDir);

    // Normalize an archive entry path: replace '\' with '/', strip drive
    // letters and leading '/', collapse "." and ".." components.
    // 规范化压缩包条目路径：将 '\' 替换为 '/'，去掉盘符与前导 '/'，
    // 并折叠 "." 与 ".." 路径分量。
    static QString normalizeEntryPath(const QString &path);

    // Check if an entry path would escape the destination (Zip Slip).
    // 检查条目路径是否会逃逸目标目录（Zip Slip）。
    static bool isPathTraversal(const QString &entryPath, const QString &destDir);

    // Whether a process with the given PID is still running.  Used to clean
    // preview/drag staging directories left by crashed instances.
    // 指定 PID 的进程是否仍在运行。用于清理崩溃实例遗留的预览/拖出暂存目录。
    static bool isProcessRunning(qint64 pid);

    // Generate a non-existing sibling path: "name (1).ext", "name (2).ext", ...
    // 生成不存在的同级路径："name (1).ext"、"name (2).ext"……
    static QString uniqueDestinationPath(const QString &path);
};

#endif // FILEUTILS_H
