#ifndef ARCHIVEMANAGER_H
#define ARCHIVEMANAGER_H

#include <QObject>
#include <QThread>

#include <functional>

#include "core/ArchiveConstants.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveEntry.h"
#include "core/ArchiveError.h"

class ArchiveBackend;
class QTemporaryDir;

// Core controller/scheduler (design doc 4.3)
// Detects formats asynchronously, owns the backends on a dedicated worker
// thread, and serializes all blocking archive operations.
class ArchiveManager : public QObject {
    Q_OBJECT
public:
    static ArchiveManager &instance();
    ~ArchiveManager() override;

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

    // Cooperative cancellation from the UI.
    void cancelCurrentOperation();
    void cancelPendingOperation();

    bool isBusy() const { return m_busy; }

    // Current backend (nullptr if no archive open)
    ArchiveBackend *backend() const { return m_backend; }
    // Current archive path
    QString currentPath() const { return m_archivePath; }
    // Current format
    ArchiveFormat currentFormat() const { return m_format; }
    // Last listed entries (empty before listing)
    QList<ArchiveEntry> entries() const { return m_entries; }

    // Create a backend for a given format (factory)
    static ArchiveBackend *createBackend(ArchiveFormat fmt);

signals:
    // Emitted before asynchronous format detection starts.
    void archiveOpening(const QString &filePath);

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

    bool startOperation(ArchiveBackend *backend,
                        std::function<void(ArchiveBackend *)> operation);
    void connectBackend(ArchiveBackend *backend, quint64 generation);
    void connectTempBackend(ArchiveBackend *backend);

    void finishOpen(const QString &filePath, ArchiveFormat format);
    void startExtractOperation(const ExtractOptions &options);
    bool validateExtraction(const ExtractOptions &options);

    void handleTerminal(ArchiveBackend *backend, bool success, const QString &msg);
    void handleError(ArchiveBackend *backend, ArchiveErrorCode code, const QString &msg);

    void cleanupTempBackend();
    void cleanupCompressSession();
    bool promoteCompressedArchive();
    void removeFailedCompressOutput();

    QThread m_workerThread;
    ArchiveBackend *m_backend = nullptr;
    ArchiveBackend *m_tempBackend = nullptr;
    QString m_archivePath;
    ArchiveFormat m_format = ArchiveFormat::Unknown;

    quint64 m_generation = 0;
    bool m_busy = false;
    bool m_opening = false;
    QList<ArchiveEntry> m_entries;

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

    // "Test before extract" chain
    bool m_preTestPending = false;
    ExtractOptions m_preTestExtractOptions;

    // Compression is written to a temporary location when the target already
    // exists, then promoted atomically on success.
    QTemporaryDir *m_compressTempDir = nullptr;
    QString m_compressFinalPath;
    bool m_compressTargetExisted = false;
};

#endif // ARCHIVEMANAGER_H
