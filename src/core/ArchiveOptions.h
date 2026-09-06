#ifndef ARCHIVEOPTIONS_H
#define ARCHIVEOPTIONS_H

#include <QString>
#include <QStringList>
#include "ArchiveConstants.h"

// Options for listContents()
struct ListOptions {
    QString     password;        // Password for encrypted filename archives
    QString     encodingHint;   // Filename encoding hint (tar/unrar only)
};

// Options for testArchive()
struct TestOptions {
    QString     password;        // Password for encrypted archives
};

// Options for extract()
struct ExtractOptions {
    QString         destDir;             // Target directory (must exist and be writable)
    QStringList     selectedFiles;       // Extract only these entries; empty = whole archive
    bool            preservePaths = true;// Whether to preserve directory structure
    OverwritePolicy overwrite = OverwritePolicy::Ask; // Overwrite policy
    QString         password;            // Password for encrypted archives
    QString         encodingHint;        // Filename encoding hint (tar/unrar only)
    bool            openDestAfterFinish = false; // Open destination after extraction
    bool            testBeforeExtract = false;   // Pre-test integrity before extraction
};

// Options for compress()
struct CompressOptions {
    QString     archivePath;            // Target archive path
    QStringList sourceFiles;            // Files/dirs to compress
    int         compressionLevel = 6;   // 0-9
    QString     password;              // Encryption password (empty = no encryption)
    bool        encryptHeaders = false;// Encrypt filenames (7z only)
    qint64      volumeSize = 0;        // Volume size in bytes (0 = no split)
    bool        solidMode = false;     // Solid compression (7z only)
    QStringList excludePatterns;       // Exclude wildcards
    bool        followSymlinks = false;// Follow symlinks
    bool        storeFullPath = false; // Store absolute paths
};

// Options for addFiles()
struct AddFilesOptions {
    QString     archivePath;            // Existing archive path
    QStringList sourceFiles;            // Files/dirs to add
    QString     password;              // Original archive password (encrypted archives)
    int         compressionLevel = 6;   // Compression level for new files
    bool        followSymlinks = false;
    bool        storeFullPath = false;
};

// Options for deleteFiles()
struct DeleteFilesOptions {
    QString     archivePath;        // Existing archive path
    QStringList targetEntries;      // Entries to delete (relative paths)
    QString     password;          // Original archive password (encrypted archives)
};

#endif // ARCHIVEOPTIONS_H
