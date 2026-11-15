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

    // Atomically move (rename) a file; cross-filesystem fallback to copy+delete.
    // Symlinks are preserved as symlinks.
    static bool moveFileAtomically(const QString &src, const QString &dest);

    // Move everything inside srcDir into destDir.
    // Directories are recreated with mkpath instead of renamed (renaming a
    // directory would invalidate the source paths of all of its children).
    // When preservePaths is false, only the basename of each entry is kept.
    // The source directory is removed only when every entry was moved.
    // Returns the number of entries that could not be moved.
    static int moveExtractedContents(const QString &srcDir, const QString &destDir,
                                     OverwritePolicy overwrite,
                                     bool preservePaths = true);

    // Create a temporary extraction directory: destDir/.tmp_extract_<uuid>/
    static QString createTempExtractDir(const QString &destDir);

    // Normalize an archive entry path: replace '\' with '/', strip drive
    // letters and leading '/', collapse "." and ".." components.
    static QString normalizeEntryPath(const QString &path);

    // Check if an entry path would escape the destination (Zip Slip).
    static bool isPathTraversal(const QString &entryPath, const QString &destDir);

    // Generate a non-existing sibling path: "name (1).ext", "name (2).ext", ...
    static QString uniqueDestinationPath(const QString &path);

    // Mask a password in command arguments (replace -p<pwd> with -p***)
    static QString maskPassword(const QString &args);
};

#endif // FILEUTILS_H
