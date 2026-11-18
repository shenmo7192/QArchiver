#ifndef ARCHIVEMANAGER_H
#define ARCHIVEMANAGER_H

// QArchiver / 小Q解压
// Central controller that schedules archive operations on a worker thread.
// 在工作线程中调度压缩包操作的核心控制器。

#include <QObject>
#include <QThread>

#include <functional>
#include <memory>
#include <variant>

#include "core/ArchiveConstants.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveEntry.h"
#include "core/ArchiveError.h"

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
    // Last listed entries (empty before listing) / 最近一次列表结果（列表前为空）
    QList<ArchiveEntry> entries() const { return m_entries; }

    // Create a backend for a given format (factory). Always uses the command
    // line tools; this is the factory for write operations.
    // 为指定格式创建后端（工厂）。始终使用命令行工具，用于写操作。
    static ArchiveBackend *createBackend(ArchiveFormat fmt);
    // Create a backend for reading an existing archive: libarchive when it is
    // available and the archive is not a split volume, otherwise the CLI.
    // 为已有压缩包创建读取后端：可用且非分卷时使用 libarchive，否则使用 CLI。
    static ArchiveBackend *createReadBackend(ArchiveFormat fmt,
                                             const QString &path = QString());
    // Whether this build links libarchive (compile-time capability).
    // 当前构建是否链接 libarchive（编译期能力）。
    static bool isLibArchiveAvailable();
    // Troubleshooting escape hatch: QARCHIVER_NO_LIBARCHIVE forces the CLI.
    // 故障排查开关：QARCHIVER_NO_LIBARCHIVE 强制使用 CLI。
    static bool isLibArchiveDisabledByEnvironment();
    // Whether createReadBackend() would pick the in-process backend for this
    // archive (preference, environment, format whitelist, split volumes).
    // createReadBackend() 是否会为该压缩包选择进程内后端
    // （取决于偏好、环境变量、格式白名单与分卷情况）。
    static bool usesLibArchiveForRead(ArchiveFormat fmt, const QString &path);

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

    void finishOpen(const QString &filePath, ArchiveFormat format);
    void startExtractOperation(const ExtractOptions &options);
    bool validateExtraction(const ExtractOptions &options);

    void handleTerminal(ArchiveBackend *backend, bool success, const QString &msg);
    void handleError(ArchiveBackend *backend, ArchiveErrorCode code, const QString &msg);
    // Retries the pending read operation on the CLI backend when the libarchive
    // backend could not handle the archive. Returns true when a retry started.
    bool tryLibArchiveFallback(ArchiveBackend *backend, ArchiveErrorCode code,
                              const QString &msg);

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

    // libarchive dispatch state: which backend serves the current archive and
    // whether the one-time fallback to the CLI backend has already happened.
    bool m_usingLibArchive = false;
    bool m_fallbackDone = false;
    bool m_fallbackInProgress = false;

    // Password retry state. The variant holds the operation that needs a
    // password; std::monostate means there is no pending retry.
    std::variant<std::monostate, ListOptions, ExtractOptions, TestOptions,
                 AddFilesOptions, DeleteFilesOptions> m_pending;
    int m_retryCount = 0;

    // "Test before extract" chain
    bool m_preTestPending = false;
    ExtractOptions m_preTestExtractOptions;

    // Compression is written to a temporary location when the target already
    // exists, then promoted atomically on success.
    std::unique_ptr<QTemporaryDir> m_compressTempDir;
    QString m_compressFinalPath;
    bool m_compressTargetExisted = false;
};

#endif // ARCHIVEMANAGER_H
