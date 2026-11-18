#ifndef ARCHIVEENTRY_H
#define ARCHIVEENTRY_H

// QArchiver / 小Q解压
// Value type describing one entry inside an archive.
// 描述压缩包内单个条目的值类型。

#include <QString>
#include <QDateTime>
#include <QList>
#include <QMetaType>

// A single entry in an archive's file list.
// 压缩包文件列表中的单个条目。
struct ArchiveEntry {
    QString     path;          // Relative path inside the archive
    qint64      size = 0;      // Uncompressed size (bytes)
    qint64      packedSize = 0;// Compressed size (bytes)
    QDateTime   mtime;         // Modification time
    bool        isDir = false; // Whether the entry is a directory
    bool        isEncrypted = false; // Whether the entry is encrypted
    bool        isSymLink = false;   // Whether the entry is a symlink
};

Q_DECLARE_METATYPE(ArchiveEntry)
Q_DECLARE_METATYPE(QList<ArchiveEntry>)

#endif // ARCHIVEENTRY_H
