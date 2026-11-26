#ifndef EXTERNALEDITMANAGER_H
#define EXTERNALEDITMANAGER_H

// QArchiver / 小Q解压
// Watches files opened from an archive in the system editor and offers to save
// the changes back into the archive.
// 监视从压缩包中用系统编辑器打开的文件，并询问是否把更改写回压缩包。

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>

#include "common/ArchiveConstants.h"
#include "common/ArchiveOptions.h"

class QTimer;
class QTemporaryDir;

// "Edit a member and save it back" (design doc 8.10).
//
// PreviewManager extracts a clicked entry into a cache directory that mirrors
// the archive tree and opens it with the system default application.  This
// manager remembers which archive member every opened file came from, polls the
// file for outside changes and - once the editor finished writing - asks the
// user whether the change should be stored in the archive.  Saving runs an
// ordinary addFiles() with AddFilesOptions::sourceRoot set, so the edited member
// replaces itself instead of being appended under a new name.
//
// Polling instead of QFileSystemWatcher: editors usually save atomically
// (temporary file + rename), which replaces the inode and silently stops a
// watcher on the old file.
//
// “编辑压缩包内文件并写回”（设计文档 8.10）。
//
// PreviewManager 会把点击的条目解压到镜像压缩包目录结构的缓存目录中，并用系统
// 默认程序打开。本类记录每个被打开文件来自哪个压缩包成员，轮询文件是否有外部
// 修改，并在编辑器写完后询问用户是否把更改存回压缩包。保存使用普通
// addFiles() 并把 AddFilesOptions::sourceRoot 设为暂存根目录，使被编辑的成员
// 替换自身，而不是以新名称追加。
//
// 使用轮询而不是 QFileSystemWatcher：编辑器通常以“临时文件 + 重命名”的方式
// 原子保存，会替换 inode 并静默失效旧文件的监视器。
class ExternalEditManager : public QObject {
    Q_OBJECT
public:
    // What the user answered in the confirmation dialog.
    // 用户在确认对话框中的选择。
    enum class Decision {
        Save,     // Store the changes in the archive / 把更改保存到压缩包
        Discard   // Leave the archive untouched / 不更新压缩包
    };

    // One tracked file. / 一个被监视的文件。
    struct Job {
        QString localPath;      // Opened file inside the preview cache / 预览缓存中的文件
        QString baseDir;        // Preview root = archive root / 预览根目录（= 压缩包根）
        QString entryPath;      // Archive member path / 压缩包内成员路径
        QString archivePath;
        QString password;
        QString nameEncoding = QStringLiteral("UTF-8");
        ArchiveFormat format = ArchiveFormat::Unknown;
        qint64 size = 0;
        QDateTime mtime;
        // Hash of the content at the last known state, used to ignore editors
        // that rewrite an unchanged file.  Empty when hashing was skipped.
        // 上次已知状态的内容哈希，用于忽略“重写但内容未变”的编辑器行为。
        // 跳过哈希时为空。
        QByteArray contentHash;

        enum class State {
            Watching,      // Polling for changes / 轮询更改中
            Asking,        // Confirmation dialog is open / 确认对话框已打开
            Saving         // Write-back is running / 写回进行中
        };
        State state = State::Watching;

        qint64 pendingSize = -1;       // Candidate change seen once / 已观察一次的候选更改
        QDateTime pendingMtime;
        qint64 pendingSinceMs = 0;
        std::shared_ptr<QTemporaryDir> staging; // Legacy-code-page staging / 旧代码页暂存目录
    };

    // Shown by the UI while the dialog runs on the GUI thread.
    // 对话框在 GUI 线程运行时由界面实现。
    using PromptHandler = std::function<Decision(const Job &job)>;
    // Starts the write-back; the UI calls confirmSaved()/reportSaveFailed()
    // when the archive operation finishes.
    // 启动写回；写回操作结束时由界面调用 confirmSaved()/reportSaveFailed()。
    using SaveHandler = std::function<void(const Job &job,
                                           const AddFilesOptions &options)>;

    static ExternalEditManager &instance();

    void setPromptHandler(PromptHandler handler);
    void setSaveHandler(SaveHandler handler);

    // Track a file that was just opened with the system application.
    // @p nameEncoding is the archive's member-name encoding.
    // 记录刚刚用系统程序打开的文件。@p nameEncoding 为压缩包成员名编码。
    void track(const QString &localPath, const QString &baseDir,
               const QString &entryPath, const QString &archivePath,
               ArchiveFormat format, const QString &password,
               const QString &nameEncoding);

    // Stop watching one file (no further prompts). / 停止监视某个文件。
    void forget(const QString &localPath);

    // The write-back succeeded: refresh the baseline and keep watching.
    // 写回成功：刷新基线并继续监视。
    void confirmSaved(const QString &localPath);
    // The write-back failed: refresh the baseline so the user is not asked
    // again immediately, and keep watching.
    // 写回失败：刷新基线，避免立刻重复询问，并继续监视。
    void reportSaveFailed(const QString &localPath);

    // Poll immediately (also used by tests). / 立即轮询（测试也会使用）。
    void checkNow();

    // A shorter settle delay makes the state machine testable.
    // 更短的稳定延迟可让状态机可被测试。
    void setSettleDelayMs(int ms);

    // Number of tracked files (tests/diagnostics). / 被监视文件的数量。
    int trackedCount() const { return int(m_jobs.size()); }

    // Tracked state of @p localPath (empty when unknown). / 指定文件的监视状态。
    bool hasJob(const QString &localPath) const
    {
        return m_jobs.contains(localPath);
    }

private:
    ExternalEditManager();
    Q_DISABLE_COPY(ExternalEditManager)

    static QByteArray hashFile(const QString &path, qint64 size);
    // Adopt the current on-disk state as the new baseline.
    // 把当前磁盘状态作为新基线。
    void adoptBaseline(Job &job);
    void askUser(const QString &localPath);
    bool prepareSave(Job &job, AddFilesOptions *options);
    void releaseStaging(Job &job);

    QHash<QString, Job> m_jobs;
    QTimer *m_timer = nullptr;
    PromptHandler m_promptHandler;
    SaveHandler m_saveHandler;
    int m_settleDelayMs = 1500;
};

#endif // EXTERNALEDITMANAGER_H
