#ifndef ARCHIVEOPTIONS_H
#define ARCHIVEOPTIONS_H

// QArchiver / 小Q解压
// Option structures shared by list, extract, compress, add and delete operations.
// 列表、解压、压缩、追加与删除操作共用的选项结构。

#include <QSet>
#include <QString>
#include <QStringList>
#include "common/ArchiveConstants.h"

// Options for listContents() / listContents() 选项
struct ListOptions {
    QString     password;        // Password for encrypted filename archives
    QString     encodingHint;   // Filename encoding hint (legacy archive names)
};

// Options for testArchive() / testArchive() 选项
struct TestOptions {
    QString     password;        // Password for encrypted archives
};

// Options for extract() / extract() 选项
struct ExtractOptions {
    QString         destDir;             // Target directory (must exist and be writable)
    QStringList     selectedFiles;       // Extract only these entries; empty = whole archive
    bool            preservePaths = true;// Whether to preserve directory structure
    OverwritePolicy overwrite = OverwritePolicy::Ask; // Overwrite policy
    QString         password;            // Password for encrypted archives
    QString         encodingHint;        // Filename encoding hint (legacy archive names)
    bool            openDestAfterFinish = false; // Open destination after extraction
    bool            testBeforeExtract = false;   // Pre-test integrity before extraction
    // Unsafe symlinks the user explicitly chose to skip.  Paths are normalized
    // archive paths; descendants of a skipped directory symlink are skipped too.
    // 用户明确选择跳过的危险符号链接。路径为规范化压缩包路径；
    // 被跳过的目录符号链接的后代也会一并跳过。
    QSet<QString>   skippedSymlinks;
    // Internal: set after the UI preflight so password retries do not prompt
    // again for the same unsafe symlinks.
    // 内部标志：界面预检完成后设置，避免密码重试时再次询问同一批危险链接。
    bool            symlinkPreflightDone = false;
};

// Options for compress() / compress() 选项
struct CompressOptions {
    QString     archivePath;            // Target archive path
    QStringList sourceFiles;            // Files/dirs to compress
    int         compressionLevel = 6;   // 0-9
    QString     password;              // Encryption password (empty = no encryption)
    bool        encryptHeaders = false;// Encrypt filenames (7z/RAR)
    qint64      volumeSize = 0;        // Volume size in bytes (0 = no split)
    bool        solidMode = false;     // Solid compression (7z/RAR)
    QStringList excludePatterns;       // Exclude wildcards
    bool        followSymlinks = false;// Follow symlinks
    bool        storeFullPath = false; // Store absolute paths
};

// Options for addFiles() / addFiles() 选项
struct AddFilesOptions {
    QString     archivePath;            // Existing archive path
    QStringList sourceFiles;            // Files/dirs to add
    QString     password;              // Original archive password (encrypted archives)
    int         compressionLevel = 6;   // Compression level for new files
    bool        followSymlinks = false;
    bool        storeFullPath = false;
};

// Options for deleteFiles() / deleteFiles() 选项
struct DeleteFilesOptions {
    QString     archivePath;        // Existing archive path
    QStringList targetEntries;      // Entries to delete (relative paths)
    QString     password;          // Original archive password (encrypted archives)
};

#endif // ARCHIVEOPTIONS_H
