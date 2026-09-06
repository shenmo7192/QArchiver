#ifndef ARCHIVEMANAGER_H
#define ARCHIVEMANAGER_H

#include <QObject>
#include <QPointer>
#include "core/ArchiveConstants.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveEntry.h"
#include "core/ArchiveError.h"

class ArchiveBackend;
class ArchiveWorker;

// Core controller/scheduler (design doc 4.3)
// Detects format, creates the appropriate backend, and dispatches async work.
class ArchiveManager : public QObject {
    Q_OBJECT
public:
    static ArchiveManager &instance();

    // Open an archive (format detection + backend creation)
    void openArchive(const QString &filePath);
    // List contents
    void listContents(const ListOptions &options = {});
    // Extract
    void extract(const ExtractOptions &options);
    // Compress (format chosen by caller; creates a fresh backend)
    void compress(const CompressOptions &options, ArchiveFormat fmt);
    // Add files to current archive
    void addFiles(const AddFilesOptions &options);
    // Delete files from current archive
    void deleteFiles(const DeleteFilesOptions &options);
    // Test current archive
    void testArchive(const TestOptions &options = {});
    // Close current archive
    void closeArchive();
    // Retry the pending operation with a password obtained from the UI
    void retryWithPassword(const QString &password);

    // Current backend (nullptr if no archive open)
    ArchiveBackend *backend() const { return m_backend; }
    // Current archive path
    QString currentPath() const { return m_archivePath; }
    // Current format
    ArchiveFormat currentFormat() const { return m_format; }

    // Create a backend for a given format (factory)
    static ArchiveBackend *createBackend(ArchiveFormat fmt);

signals:
    // Forwarded from backend
    void archiveOpened(const QString &filePath);
    void fileListReady(const QList<ArchiveEntry> &entries);
    void progressChanged(int percent, int total, int current, int etaSec);
    void outputReady(const QString &line);
    void passwordRequested();
    void errorOccurred(ArchiveErrorCode code, const QString &msg);
    void operationFinished(bool success, const QString &msg);

private:
    ArchiveManager();
    Q_DISABLE_COPY(ArchiveManager)

    void connectBackend(ArchiveBackend *backend);
    void deleteBackend();

    ArchiveBackend *m_backend = nullptr;
    ArchiveWorker *m_worker = nullptr;
    QString m_archivePath;
    ArchiveFormat m_format = ArchiveFormat::Unknown;

    // Password retry state
    enum class PendingOp { None, List, Extract, Test, Add, Delete };
    PendingOp m_pendingOp = PendingOp::None;
    int m_retryCount = 0;

    // Cached options for retry with a password
    ListOptions m_pendingListOptions;
    ExtractOptions m_pendingExtractOptions;
    TestOptions m_pendingTestOptions;
    AddFilesOptions m_pendingAddOptions;
    DeleteFilesOptions m_pendingDeleteOptions;
};

#endif // ARCHIVEMANAGER_H
