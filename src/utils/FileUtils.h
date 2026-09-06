#ifndef FILEUTILS_H
#define FILEUTILS_H

#include <QString>
#include <QStringList>
#include <QList>

#include "core/ArchiveConstants.h"

struct ArchiveEntry;

// File/path utility functions
class FileUtils {
public:
    // Canonicalize a path (resolve symlinks, remove .. and .)
    static QString canonicalPath(const QString &path);

    // Estimate free disk space (bytes) at the given path; -1 on failure
    static qint64 estimateFreeSpace(const QString &path);

    // Count all files (recursively) under the given paths
    static int countFiles(const QStringList &paths);

    // Estimate the total uncompressed size of an archive's entries
    static qint64 totalUnpackedSize(const QList<ArchiveEntry> &entries);

    // Recursively remove a directory and all contents
    static bool removeDirectoryRecursively(const QString &path);

    // Atomically move (rename) a file; cross-filesystem fallback to copy+delete
    static bool moveFileAtomically(const QString &src, const QString &dest);

    // Move everything inside srcDir into destDir, recreating directories with
    // mkpath instead of renaming them (renaming a directory would invalidate
    // the source paths of all of its children). Symlinks are moved as links.
    // Returns the number of entries that could not be moved.
    static int moveExtractedContents(const QString &srcDir, const QString &destDir,
                                     OverwritePolicy overwrite);

    // Create a temporary extraction directory: destDir/.tmp_extract_<uuid>/
    static QString createTempExtractDir(const QString &destDir);

    // Normalize an archive entry path: clean path, strip leading '/', reject '../'
    static QString normalizeEntryPath(const QString &path);

    // Check if an entry path would escape the destination (Zip Slip)
    static bool isPathTraversal(const QString &entryPath, const QString &destDir);

    // Mask a password in command arguments (replace -p<pwd> with -p***)
    static QString maskPassword(const QString &args);
};

#endif // FILEUTILS_H
