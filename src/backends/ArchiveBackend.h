#ifndef ARCHIVEBACKEND_H
#define ARCHIVEBACKEND_H

// QArchiver / 小Q解压
// Abstract interface implemented by every archive backend.
// 所有压缩包后端实现的抽象接口。

#include <QObject>
#include <QString>
#include <QList>

#include <atomic>

#include "common/ArchiveConstants.h"
#include "common/ArchiveOptions.h"
#include "common/ArchiveEntry.h"
#include "common/ArchiveError.h"

// Abstract backend interface (design doc 4.1).
// All backends must implement this unified interface.
// 抽象后端接口（设计文档 4.1）。
// 所有后端都必须实现这套统一接口。
class ArchiveBackend : public QObject {
    Q_OBJECT
public:
    explicit ArchiveBackend(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~ArchiveBackend() = default;

    // Core operations / 核心操作
    // Lightweight binding: existence check + backend association (no password, no content read)
    // 轻量绑定：检查文件存在并关联后端（不读密码、不读取内容）
    virtual void openArchive(const QString &filePath) = 0;
    // List entries (password first requested here for encrypted-filename archives)
    // 列出条目（加密文件名的压缩包会在此首次请求密码）
    virtual void listContents(const ListOptions &options = {}) = 0;
    // Extract / 解压
    virtual void extract(const ExtractOptions &options) = 0;
    // Compress / 压缩
    virtual void compress(const CompressOptions &options) = 0;
    // Add files to existing archive (encrypted archives need password)
    // 向已有压缩包追加文件（加密压缩包需要密码）
    virtual void addFiles(const AddFilesOptions &options) = 0;
    // Delete entries from existing archive / 从已有压缩包删除条目
    virtual void deleteFiles(const DeleteFilesOptions &options) = 0;
    // Test integrity (encrypted archives need password) / 测试完整性（加密包需要密码）
    virtual void testArchive(const TestOptions &options = {}) = 0;
    // Release file handles and clean up temp resources / 释放文件句柄并清理临时资源
    virtual void closeArchive() = 0;

    // Read-only capability queries (for UI to decide available operations)
    // 只读能力查询（供界面决定可用操作）
    virtual ArchiveFormat format() const = 0;
    virtual bool isReadOnly() const = 0;
    virtual bool supportsEncryption() const = 0;
    virtual bool supportsVolume() const = 0;
    // Runtime decision: uncompressed .tar / single-volume zip|7z -> true
    // 运行时判断：未压缩 .tar 或单卷 zip/7z 时返回 true
    virtual bool supportsAppend() const = 0;

    // Current archive path / 当前压缩包路径
    QString archivePath() const { return m_archivePath; }

    // Encoding of legacy entry names as detected (or imposed by the caller) by
    // the backend, e.g. "UTF-8" or "GB18030".  Callers reuse this value for
    // extraction and drag-out so member names are decoded exactly like the
    // listing that produced them.  Empty means "not detected yet".
    // 后端检测（或调用方指定）的旧式条目名编码，例如 "UTF-8"、"GB18030"。
    // 解压与拖出复用该值，保证成员名按与列表一致的方式解码。
    // 返回空串表示尚未检测。
    virtual QString nameEncoding() const { return QString(); }

    // Cooperative cancellation: the manager sets this flag from the GUI
    // thread; long-running backends poll it between process I/O operations.
    // 协作式取消：管理器从 GUI 线程设置该标志；
    // 长时间运行的后端在进程 I/O 之间轮询。
    void requestCancel() noexcept { m_cancelRequested.store(true, std::memory_order_relaxed); }
    void clearCancel() noexcept { m_cancelRequested.store(false, std::memory_order_relaxed); }
    bool isCancelled() const noexcept { return m_cancelRequested.load(std::memory_order_relaxed); }

signals:
    // Operation lifecycle and progress notifications. / 操作生命周期与进度通知。
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
    std::atomic_bool m_cancelRequested{false};
};

#endif // ARCHIVEBACKEND_H
