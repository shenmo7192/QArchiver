// QArchiver / 小Q解压
// FileUtils implementation: path normalization, traversal checks and moves.
// FileUtils 实现：路径规范化、路径穿越检查与文件移动。

#include "utils/FileUtils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QUuid>

#include <limits>

#include "common/ArchiveEntry.h"

#ifndef Q_OS_WIN
#include <unistd.h>
#else
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

QString rawSymlinkTarget(const QString &path)
{
#ifndef Q_OS_WIN
    const QByteArray nativePath = QFile::encodeName(path);
    QByteArray buffer(4096, Qt::Uninitialized);
    for (;;) {
        const ssize_t size = ::readlink(nativePath.constData(), buffer.data(),
                                        static_cast<size_t>(buffer.size()));
        if (size < 0) {
            return QString();
        }
        if (size < buffer.size()) {
            buffer.truncate(static_cast<int>(size));
            return QString::fromLocal8Bit(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
#else
    return QFileInfo(path).symLinkTarget();
#endif
}

bool isAbsoluteLinkTarget(const QString &target)
{
    if (target.isEmpty()) {
        return true;
    }
    if (QDir::isAbsolutePath(target)
        || target.startsWith(QLatin1Char('/'))
        || target.startsWith(QLatin1Char('\\'))) {
        return true;
    }
    static const QRegularExpression driveRe(QStringLiteral("^[A-Za-z]:"));
    return driveRe.match(target).hasMatch();
}

QStringList pathComponents(const QString &path)
{
    return path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
}

bool copyMetadata(const QString &src, const QString &dest)
{
    const QFileInfo info(src);
    if (info.isSymLink()) {
        return true;
    }

    QFile destFile(dest);
    if (!destFile.open(QIODevice::ReadOnly)) {
        return false;
    }
    destFile.setFileTime(info.lastModified(), QFileDevice::FileModificationTime);
    destFile.close();

    return QFile::setPermissions(dest, info.permissions());
}

} // namespace

QString FileUtils::canonicalPath(const QString &path)
{
    QFileInfo fi(path);
    const QString canon = fi.canonicalFilePath();
    if (canon.isEmpty()) {
        // File doesn't exist yet; canonicalize the parent.
        // 文件尚不存在；改为规范化其父目录。
        return QDir::cleanPath(path);
    }
    return canon;
}

qint64 FileUtils::estimateFreeSpace(const QString &path)
{
    QString dir = path;
    QFileInfo fi(dir);
    if (fi.isFile()) {
        dir = fi.absolutePath();
    }

    // Walk up to the first existing directory. / 向上查找第一个存在的目录。
    while (!dir.isEmpty() && !QFileInfo::exists(dir)) {
        const QString parent = QFileInfo(dir).absolutePath();
        if (parent == dir) {
            break;
        }
        dir = parent;
    }

    QStorageInfo storage(dir);
    if (!storage.isValid() || !storage.isReady()) {
        return -1;
    }
    return storage.bytesAvailable();
}

int FileUtils::countFiles(const QStringList &paths)
{
    int count = 0;
    for (const QString &path : paths) {
        QFileInfo fi(path);
        if (!fi.exists()) {
            continue;
        }
        if (fi.isFile()) {
            ++count;
        } else if (fi.isDir()) {
            QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                ++count;
            }
        }
    }
    return count;
}

bool FileUtils::addSizeChecked(qint64 &total, qint64 value)
{
    if (value < 0) {
        return false;
    }
    if (total > std::numeric_limits<qint64>::max() - value) {
        return false;
    }
    total += value;
    return true;
}

qint64 FileUtils::totalUnpackedSize(const QList<ArchiveEntry> &entries)
{
    qint64 total = 0;
    for (const ArchiveEntry &entry : entries) {
        if (entry.isDir) {
            continue;
        }
        if (!addSizeChecked(total, entry.size)) {
            return std::numeric_limits<qint64>::max();
        }
    }
    return total;
}

bool FileUtils::totalUnpackedSizeChecked(const QList<ArchiveEntry> &entries,
                                         qint64 *total)
{
    if (!total) {
        return false;
    }
    qint64 sum = 0;
    for (const ArchiveEntry &entry : entries) {
        if (entry.isDir) {
            continue;
        }
        if (!addSizeChecked(sum, entry.size)) {
            return false;
        }
    }
    *total = sum;
    return true;
}

bool FileUtils::removeDirectoryRecursively(const QString &path)
{
    QDir dir(path);
    return dir.removeRecursively();
}

bool FileUtils::moveFileAtomically(const QString &src, const QString &dest)
{
    QFile srcFile(src);

    // Same-filesystem rename preserves permissions, timestamps and symlinks.
    // 同一文件系统内重命名可保留权限、时间戳和符号链接。
    if (srcFile.rename(dest)) {
        return true;
    }

    const QFileInfo srcInfo(src);
    if (srcInfo.isSymLink()) {
        const QString target = srcInfo.symLinkTarget();
        QFile::remove(dest);
        if (QFile::link(target, dest)) {
            return QFile::remove(src);
        }
        return false;
    }

    // Cross-filesystem fallback: copy then delete. / 跨文件系统回退：复制后删除。
    if (QFile::copy(src, dest)) {
        copyMetadata(src, dest);
        return QFile::remove(src);
    }
    return false;
}

int FileUtils::moveExtractedContents(const QString &srcDir, const QString &destDir,
                                     OverwritePolicy overwrite, bool preservePaths)
{
    QDir src(srcDir);
    QDir().mkpath(destDir);

    // Snapshot first: moving files while QDirIterator walks the tree can skip
    // entries on some filesystems.
    // 先做快照：在 QDirIterator 遍历时移动文件，某些文件系统可能漏掉条目。
    struct Item {
        QString source;
        QString relative;
        bool isDir = false;
        bool isSymLink = false;
    };
    QList<Item> items;

    QDirIterator it(srcDir,
                    QDir::Files | QDir::Dirs | QDir::System
                        | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        items.append({it.filePath(), src.relativeFilePath(it.filePath()),
                      info.isDir(), info.isSymLink()});
    }

    int failed = 0;
    for (const Item &item : items) {
        QString relative = preservePaths
            ? item.relative
            : QFileInfo(item.relative).fileName();
        if (relative.isEmpty()) {
            continue;
        }

        const QString destPath = QDir(destDir).filePath(relative);

        if (item.isDir && !item.isSymLink) {
            if (!QDir().mkpath(destPath)) {
                ++failed;
            }
            continue;
        }

        QDir().mkpath(QFileInfo(destPath).absolutePath());

        QFileInfo destInfo(destPath);
        if (destInfo.exists() || destInfo.isSymLink()) {
            switch (overwrite) {
            case OverwritePolicy::Never:
                continue;
            case OverwritePolicy::Rename: {
                const QString renamed = uniqueDestinationPath(destPath);
                QDir().mkpath(QFileInfo(renamed).absolutePath());
                if (!moveFileAtomically(item.source, renamed)) {
                    ++failed;
                }
                continue;
            }
            case OverwritePolicy::Ask:
            case OverwritePolicy::Always:
                if (destInfo.isDir() && !destInfo.isSymLink()) {
                    removeDirectoryRecursively(destPath);
                } else {
                    QFile::remove(destPath);
                }
                break;
            }
        }

        if (!moveFileAtomically(item.source, destPath)) {
            ++failed;
        }
    }

    // Keep the temporary directory when something failed so the caller can
    // report/retry instead of silently deleting extracted data.
    // 失败时保留临时目录，便于调用方报告或重试，而不是静默删除已解压数据。
    if (failed == 0) {
        removeDirectoryRecursively(srcDir);
    }
    return failed;
}

QString FileUtils::createTempExtractDir(const QString &destDir)
{
    const QString uuid = QUuid::createUuid().toString(QUuid::Id128);
    const QString tempName = QStringLiteral(".tmp_extract_%1").arg(uuid);
    const QString tempPath = QDir(destDir).absoluteFilePath(tempName);
    QDir().mkpath(tempPath);
    return tempPath;
}

QString FileUtils::normalizeEntryPath(const QString &path)
{
    QString normalized = path;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));

    // Strip a Windows drive prefix ("C:"). / 去掉 Windows 盘符前缀（"C:"）。
    if (normalized.size() >= 2 && normalized.at(1) == QLatin1Char(':')) {
        normalized = normalized.mid(2);
    }

    normalized = QDir::cleanPath(normalized);
    while (normalized.startsWith(QLatin1Char('/'))) {
        normalized = normalized.mid(1);
    }
    return normalized;
}

bool FileUtils::isPathTraversal(const QString &entryPath, const QString &destDir)
{
    const QString normalized = normalizeEntryPath(entryPath);
    if (normalized.isEmpty()) {
        return false;
    }

    // Reject any ".." path component. Substring matching would incorrectly
    // reject legitimate names such as "foo..bar".
    // 拒绝任何 ".." 路径分量。若用子串匹配，会误判 "foo..bar" 这类合法名称。
    const QStringList components = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &component : components) {
        if (component == QStringLiteral("..")) {
            return true;
        }
    }

    const QString cleanDest = QDir::cleanPath(QDir(destDir).absolutePath());
    const QString cleanResolved = QDir::cleanPath(QDir(cleanDest).absoluteFilePath(normalized));

    if (cleanResolved == cleanDest) {
        return false;
    }
    if (!cleanResolved.startsWith(cleanDest + QLatin1Char('/'))) {
        return true;
    }

    // If the target already exists, also compare canonical paths to catch
    // symlinked parent directories that escape the destination.
    // 目标已存在时再比较规范化路径，以捕获通过符号链接父目录逃逸的情况。
    const QFileInfo resolvedInfo(cleanResolved);
    if (resolvedInfo.exists()) {
        const QString canonicalDest = QFileInfo(cleanDest).canonicalFilePath();
        const QString canonicalResolved = resolvedInfo.canonicalFilePath();
        if (!canonicalDest.isEmpty() && !canonicalResolved.isEmpty()
            && canonicalResolved != canonicalDest
            && !canonicalResolved.startsWith(canonicalDest + QLatin1Char('/'))) {
            return true;
        }
    }

    return false;
}


bool FileUtils::isSafeSymlinkTarget(const QString &entryPath,
                                    const QString &target)
{
    if (isAbsoluteLinkTarget(target)) {
        return false;
    }

    const QString normalizedEntry = normalizeEntryPath(entryPath);
    const int slash = normalizedEntry.lastIndexOf(QLatin1Char('/'));
    const QString parent = slash < 0 ? QString() : normalizedEntry.left(slash);

    // A relative link resolves from its parent directory.  Walk the target's
    // components and reject any ".." that would climb above the archive root.
    int depth = pathComponents(parent).size();
    QString normalizedTarget = target;
    normalizedTarget.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QStringList components = pathComponents(normalizedTarget);
    for (const QString &component : components) {
        if (component == QStringLiteral("..")) {
            if (--depth < 0) {
                return false;
            }
        } else if (component != QStringLiteral(".")) {
            ++depth;
        }
    }
    return true;
}

QStringList FileUtils::unsafeExtractedSymlinks(const QString &srcDir)
{
    QStringList unsafe;
    const QString root = QDir(srcDir).absolutePath();
    const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
    if (canonicalRoot.isEmpty()) {
        return unsafe;
    }

    QDirIterator it(root,
                    QDir::AllEntries | QDir::Hidden | QDir::System
                        | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        if (!info.isSymLink()) {
            continue;
        }

        const QString relative = QDir(root).relativeFilePath(path);
        const QString target = rawSymlinkTarget(path);
        if (!isSafeSymlinkTarget(relative, target)) {
            unsafe << normalizeEntryPath(relative);
            continue;
        }

        // Follow an existing target chain and compare canonical paths.  A
        // dangling target is allowed: it may be extracted later in the archive.
        const QString targetPath = QDir::cleanPath(
            QDir(info.absolutePath()).absoluteFilePath(target));
        const QString resolvedRelative = QDir(root).relativeFilePath(targetPath);
        if (resolvedRelative == QStringLiteral("..")
            || resolvedRelative.startsWith(QStringLiteral("../"))) {
            unsafe << normalizeEntryPath(relative);
            continue;
        }

        const QFileInfo targetInfo(targetPath);
        if (targetInfo.exists() || targetInfo.isSymLink()) {
            const QString canonicalTarget = targetInfo.canonicalFilePath();
            if (!canonicalTarget.isEmpty()
                && canonicalTarget != canonicalRoot
                && !canonicalTarget.startsWith(canonicalRoot + QLatin1Char('/'))) {
                unsafe << normalizeEntryPath(relative);
            }
        }
    }
    return unsafe;
}

bool FileUtils::isProcessRunning(qint64 pid)
{
    if (pid <= 0) {
        return false;
    }
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                static_cast<DWORD>(pid));
    if (!handle) {
        // Access denied usually means another user's process; a stale cache
        // directory is safer to remove than to leak forever.
        // 拒绝访问通常意味着其他用户的进程；清理过期缓存目录比永久泄漏更安全。
        return false;
    }
    DWORD exitCode = 0;
    const bool alive = GetExitCodeProcess(handle, &exitCode)
        && exitCode == STILL_ACTIVE;
    CloseHandle(handle);
    return alive;
#else
    return QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid));
#endif
}

QString FileUtils::uniqueDestinationPath(const QString &path)
{
    const QFileInfo info(path);
    const QString dir = info.absolutePath();
    const QString name = info.fileName();

    const int dot = name.lastIndexOf(QLatin1Char('.'));
    const QString base = dot > 0 ? name.left(dot) : name;
    const QString suffix = dot > 0 ? name.mid(dot) : QString();

    for (int index = 1; index < 100000; ++index) {
        const QString candidate = QDir(dir).filePath(
            QStringLiteral("%1 (%2)%3").arg(base).arg(index).arg(suffix));
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return path;
}
