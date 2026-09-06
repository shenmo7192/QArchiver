#ifndef ARCHIVEBACKEND_H
#define ARCHIVEBACKEND_H

#include <QObject>
#include <QString>
#include <QList>
#include "core/ArchiveConstants.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveEntry.h"
#include "core/ArchiveError.h"

// Abstract backend interface (design doc 4.1)
// All backends must implement this unified interface.
class ArchiveBackend : public QObject {
    Q_OBJECT
public:
    explicit ArchiveBackend(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ArchiveBackend() = default;

    // Core operations
    // Lightweight binding: existence check + backend association (no password, no content read)
    virtual void openArchive(const QString &filePath) = 0;
    // List entries (password first requested here for encrypted-filename archives)
    virtual void listContents(const ListOptions &options = {}) = 0;
    // Extract
    virtual void extract(const ExtractOptions &options) = 0;
    // Compress
    virtual void compress(const CompressOptions &options) = 0;
    // Add files to existing archive (encrypted archives need password)
    virtual void addFiles(const AddFilesOptions &options) = 0;
    // Delete entries from existing archive
    virtual void deleteFiles(const DeleteFilesOptions &options) = 0;
    // Test integrity (encrypted archives need password)
    virtual void testArchive(const TestOptions &options = {}) = 0;
    // Release file handles and clean up temp resources
    virtual void closeArchive() = 0;

    // Read-only capability queries (for UI to decide available operations)
    virtual ArchiveFormat format() const = 0;
    virtual bool isReadOnly() const = 0;
    virtual bool supportsEncryption() const = 0;
    virtual bool supportsVolume() const = 0;
    // Runtime decision: uncompressed .tar / single-volume zip|7z -> true
    virtual bool supportsAppend() const = 0;
    virtual QString archiveComment() const { return QString(); }

    // Current archive path
    QString archivePath() const { return m_archivePath; }

signals:
    void archiveOpened(const QString &filePath);
    void fileListReady(const QList<ArchiveEntry> &entries);
    void progressChanged(int percent, int total, int current, int etaSec);
    void outputReady(const QString &line);
    void passwordRequested();
    void errorOccurred(ArchiveErrorCode code, const QString &msg);
    void operationFinished(bool success, const QString &msg);

protected:
    QString m_archivePath;
    bool m_isOpen = false;
};

#endif // ARCHIVEBACKEND_H
