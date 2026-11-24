#ifndef ARCHIVEMANAGER_H
#define ARCHIVEMANAGER_H

// QArchiver / 小Q解压
// Central controller that schedules archive operations on a worker thread.
// 在工作线程中调度压缩包操作的核心控制器。

#include <QObject>
#include <QSet>
#include <QThread>

#include <functional>
#include <memory>
#include <variant>

#include "common/ArchiveConstants.h"
#include "common/ArchiveOptions.h"
#include "common/ArchiveEntry.h"
#include "common/ArchiveError.h"
#include "core/ExtractionPreflight.h"

class ArchiveBackend;
class QTemporaryDir;

// Core controller/scheduler (design doc 4.3).
// Detects formats asynchronously, owns the backends on a dedicated worker
// thread, and serializes all blocking archive operations.
// 核心控制器/调度器（设计文档 4.3）。
// 异步检测格式，在专用工作线程中持有后端，并串行执行所有阻塞式操作。
class ArchiveManager : public QObject {
    Q_OBJECT
public:
    static ArchiveManager &instance();
    ~ArchiveManager() override;

    // Open an archive (format detection + backend creation)
    // 打开压缩包（格式检测 + 创建后端）
    void openArchive(const QString &filePath);
    // List contents / 列出内容
    void listContents(const ListOptions &options = {});
    // Extract / 解压
    void extract(const ExtractOptions &options);
    // Compress (format chosen by caller; creates a fresh backend)
    // 压缩（格式由调用方选择；创建新的后端）
    void compress(const CompressOptions &options, ArchiveFormat fmt);
    // Add files to current archive / 向当前压缩包追加文件
    void addFiles(const AddFilesOptions &options);
    // Delete files from current archive / 从当前压缩包删除文件
    void deleteFiles(const DeleteFilesOptions &options);
    // Test current archive / 测试当前压缩包完整性
    void testArchive(const TestOptions &options = {});
    // Close current archive / 关闭当前压缩包
    void closeArchive();
    // Retry the pending operation with a password obtained from the UI
    // 使用界面获取的密码重试待处理操作
    void retryWithPassword(const QString &password);

    // Defined by the extraction preflight; re-exported so the controller API
    // stays unchanged for callers.
    // 由解压预检定义；在此重新导出，使控制器 API 对调用方保持不变。
    using SymlinkDecision = ExtractionPreflight::SymlinkDecision;
    using SymlinkDecisionCallback = ExtractionPreflight::SymlinkDecisionCallback;
    // UI hook used during extraction preflight.  Called on the GUI thread;
    // null means unsafe symlinks abort the operation.
    // 解压预检期间使用的界面回调。在 GUI 线程调用；为空表示危险符号链接
    // 会中止操作。
    void setSymlinkDecisionCallback(SymlinkDecisionCallback callback);

    // Cooperative cancellation from the UI. / 来自界面的协作式取消。
    void cancelCurrentOperation();
    void cancelPendingOperation();

    bool isBusy() const { return m_busy; }

    // Current backend (nullptr if no archive open) / 当前后端（未打开时为 nullptr）
    ArchiveBackend *backend() const { return m_backend; }
    // Current archive path / 当前压缩包路径
    QString currentPath() const { return m_archivePath; }
    // Current format / 当前格式
    ArchiveFormat currentFormat() const { return m_format; }
    // Whether add/delete can be performed now.  A read-only libarchive
    // backend is transparently reopened with the CLI backend for zip/7z/plain
    // tar, so this can be true even when backend()->supportsAppend() is false.
    // 当前是否可以添加/删除。zip/7z/未压缩 tar 的只读 libarchive 后端会透明地用
    // 命令行后端重新打开，因此即使 backend()->supportsAppend() 为 false 也可能为真。
    bool canModifyCurrentArchive() const;
    // Last listed entries (empty before listing) / 最近一次列表结果（列表前为空）
    QList<ArchiveEntry> entries() const { return m_entries; }
    // Encoding the backend used (or detected) for legacy member names.
    // Empty while no archive is open or before the first listing.
    // 后端用于（或检测出的）旧式成员名编码。未打开压缩包或尚未列表时为空。
    QString nameEncoding() const;

    // Backend creation and the libarchive read policy live in
    // ArchiveBackendFactory; the manager uses them internally.
    // 后端创建与 libarchive 读取策略位于 ArchiveBackendFactory，
    // 控制器在内部使用。

signals:
    // Emitted before asynchronous format detection starts. / 异步格式检测开始前发出。
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

    void destroyBackend(ArchiveBackend *backend);
    void clearPendingState();
    void finishOpen(const QString &filePath, ArchiveFormat format);
    void startExtractOperation(const ExtractOptions &options);
    bool validateExtraction(const ExtractOptions &options,
                            QSet<QString> *skippedSymlinks);

    void handleTerminal(ArchiveBackend *backend, bool success, const QString &msg);
    void handleError(ArchiveBackend *backend, ArchiveErrorCode code, const QString &msg);
    // Retries the pending read operation on the CLI backend when the libarchive
    // backend could not handle the archive. Returns true when a retry started.
    bool tryLibArchiveFallback(ArchiveBackend *backend, ArchiveErrorCode code,
                              const QString &msg);

    void cleanupTempBackend();
    void cleanupCompressSession();

    // libarchive is read-only.  Add/delete on a zip/7z/plain-tar archive is
    // transparently re-dispatched after reopening the same archive with the
    // command-line backend, so the modern 添加/删除 buttons keep working.
    // libarchive 是只读后端。对 zip/7z/未压缩 tar 的添加/删除会先用命令行后端
    // 重新打开同一压缩包，再重放操作，使现代界面的“添加/删除”按钮可用。
    bool canReopenForWrite() const;
    void clearWriteAfterReopen();
    bool promoteCompressedArchive();
    void removeFailedCompressOutput();

    QThread m_workerThread;
    ArchiveBackend *m_backend = nullptr;
    ArchiveBackend *m_tempBackend = nullptr;
    ArchiveBackend *m_openingBackend = nullptr;
    QString m_archivePath;
    QString m_openingPath;
    ArchiveFormat m_openingFormat = ArchiveFormat::Unknown;
    ArchiveFormat m_format = ArchiveFormat::Unknown;

    quint64 m_generation = 0;
    bool m_busy = false;
    bool m_opening = false;
    QList<ArchiveEntry> m_entries;

    // libarchive dispatch state: which backend serves the current archive and
    // whether the one-time fallback to the CLI backend has already happened.
    bool m_usingLibArchive = false;
    bool m_fallbackDone = false;
    bool m_fallbackInProgress = false;
    bool m_forceCliForOpen = false;
    std::variant<std::monostate, AddFilesOptions, DeleteFilesOptions>
        m_writeAfterReopen;

    // Password retry state. The variant holds the operation that needs a
    // password; std::monostate means there is no pending retry.
    std::variant<std::monostate, ListOptions, ExtractOptions, TestOptions,
                 AddFilesOptions, DeleteFilesOptions> m_pending;
    int m_retryCount = 0;

    // "Test before extract" chain
    bool m_preTestPending = false;
    ExtractOptions m_preTestExtractOptions;

    SymlinkDecisionCallback m_symlinkDecisionCallback;

    // Compression is written to a temporary location when the target already
    // exists, then promoted atomically on success.
    std::unique_ptr<QTemporaryDir> m_compressTempDir;
    QString m_compressFinalPath;
    ArchiveFormat m_compressFormat = ArchiveFormat::Unknown;
    bool m_compressTargetExisted = false;
};

#endif // ARCHIVEMANAGER_H
