#ifndef ARCHIVEENTRY_H
#define ARCHIVEENTRY_H

#include <QString>
#include <QDateTime>
#include <QList>
#include <QMetaType>

// A single entry in an archive's file list
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
