#include "FileUtils.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QStorageInfo>
#include <QRegularExpression>

#include "ArchiveEntry.h"

QString FileUtils::canonicalPath(const QString &path)
{
    QFileInfo fi(path);
    QString canon = fi.canonicalFilePath();
    if (canon.isEmpty()) {
        // File doesn't exist yet; canonicalize the parent
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
    QStorageInfo storage(dir);
    if (!storage.isValid() || !storage.isReady()) {
        return -1;
    }
    return storage.bytesAvailable();
}

int FileUtils::countFiles(const QStringList &paths)
{
    int count = 0;
    for (const QString &p : paths) {
        QFileInfo fi(p);
        if (!fi.exists()) continue;
        if (fi.isFile()) {
            ++count;
        } else if (fi.isDir()) {
            QDir dir(p);
            QDirIterator it(p, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                ++count;
            }
        }
    }
    return count;
}

qint64 FileUtils::totalUnpackedSize(const QList<ArchiveEntry> &entries)
{
    qint64 total = 0;
    for (const auto &e : entries) {
        total += e.size;
    }
    return total;
}

bool FileUtils::removeDirectoryRecursively(const QString &path)
{
    QDir dir(path);
    return dir.removeRecursively();
}

bool FileUtils::moveFileAtomically(const QString &src, const QString &dest)
{
    QFile srcFile(src);
    // Try atomic rename first (same filesystem)
    if (srcFile.rename(dest)) {
        return true;
    }
    // Fallback: copy then delete (cross-filesystem)
    if (srcFile.copy(dest)) {
        srcFile.remove();
        return true;
    }
    return false;
}

int FileUtils::moveExtractedContents(const QString &srcDir, const QString &destDir,
                                     OverwritePolicy overwrite)
{
    QDir src(srcDir);
    QDir().mkpath(destDir);

    QDirIterator it(srcDir,
                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    int failed = 0;
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const QString relPath = src.relativeFilePath(it.filePath());
        const QString destPath = QDir(destDir).filePath(relPath);

        // Recreate directories instead of renaming them. If we renamed a
        // directory, the source paths of all of its children would disappear
        // and every subsequent child move would fail.
        if (info.isDir() && !info.isSymLink()) {
            QDir().mkpath(destPath);
            continue;
        }

        // Everything else (regular files, symlinks, special files) is moved.
        QFileInfo destInfo(destPath);
        QDir().mkpath(destInfo.absolutePath());
        if (destInfo.exists()) {
            if (overwrite == OverwritePolicy::Never) {
                continue;
            }
            QFile::remove(destPath);
        }

        if (!moveFileAtomically(it.filePath(), destPath)) {
            ++failed;
        }
    }

    removeDirectoryRecursively(srcDir);
    return failed;
}

QString FileUtils::createTempExtractDir(const QString &destDir)
{
    const QString uuid = QUuid::createUuid().toString(QUuid::Id128);
    QString tempName = QStringLiteral(".tmp_extract_%1").arg(uuid);
    QString tempPath = QDir(destDir).absoluteFilePath(tempName);
    QDir().mkpath(tempPath);
    return tempPath;
}

QString FileUtils::normalizeEntryPath(const QString &path)
{
    QString p = QDir::cleanPath(path);
    // Strip leading '/'
    while (p.startsWith('/')) {
        p = p.mid(1);
    }
    return p;
}

bool FileUtils::isPathTraversal(const QString &entryPath, const QString &destDir)
{
    QString norm = normalizeEntryPath(entryPath);
    // Reject if still contains '..' after cleanPath
    if (norm.contains("..")) {
        return true;
    }
    // Check the resolved canonical path is within destDir
    QString resolved = QDir(destDir).absoluteFilePath(norm);
    QString canonDest = QFileInfo(destDir).canonicalFilePath();
    QString canonResolved = QFileInfo(resolved).canonicalFilePath();
    // If the file already exists, canonicalFilePath will resolve it
    if (!canonResolved.isEmpty()) {
        if (!canonResolved.startsWith(canonDest)) {
            return true;
        }
    }
    return false;
}

QString FileUtils::maskPassword(const QString &args)
{
    QRegularExpression re(QStringLiteral("-p[^ ]+"));
    QString masked = args;
    masked.replace(re, QStringLiteral("-p***"));
    return masked;
}
