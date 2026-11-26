// QArchiver / 小Q解压
// External-edit detection: polling, settle delay and write-back preparation.
// 外部编辑检测：轮询、稳定延迟与写回准备。

#include "ui/services/ExternalEditManager.h"

#include "core/AppSettings.h"
#include "utils/FileUtils.h"
#include "utils/Logger.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTimer>

namespace {

// Files larger than this are never hashed: the poll would read a huge file
// every time an editor touches it.
// 超过该大小的文件不做哈希：否则每次编辑器触碰文件都要读取大量数据。
constexpr qint64 kMaxHashBytes = 16 * 1024 * 1024;

// Polling interval of the watcher. / 监视器的轮询间隔。
constexpr int kPollIntervalMs = 1500;

bool isUtf8NameEncoding(const QString &encoding)
{
    return encoding.isEmpty()
        || encoding.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0;
}

} // namespace

ExternalEditManager::ExternalEditManager()
    : QObject(nullptr)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(kPollIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &ExternalEditManager::checkNow);
}

ExternalEditManager &ExternalEditManager::instance()
{
    static ExternalEditManager manager;
    return manager;
}

void ExternalEditManager::setPromptHandler(PromptHandler handler)
{
    m_promptHandler = std::move(handler);
}

void ExternalEditManager::setSaveHandler(SaveHandler handler)
{
    m_saveHandler = std::move(handler);
}

void ExternalEditManager::setSettleDelayMs(int ms)
{
    m_settleDelayMs = qMax(0, ms);
}

QByteArray ExternalEditManager::hashFile(const QString &path, qint64 size)
{
    if (size <= 0 || size > kMaxHashBytes) {
        return QByteArray();
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return QCryptographicHash::hash(file.readAll(), QCryptographicHash::Md5);
}

void ExternalEditManager::adoptBaseline(Job &job)
{
    const QFileInfo info(job.localPath);
    job.size = info.exists() ? info.size() : -1;
    job.mtime = info.exists() ? info.lastModified() : QDateTime();
    job.contentHash = info.exists() ? hashFile(job.localPath, job.size) : QByteArray();
    job.pendingSize = -1;
    job.pendingMtime = QDateTime();
    job.pendingSinceMs = 0;
}

void ExternalEditManager::releaseStaging(Job &job)
{
    if (!job.staging) {
        return;
    }
    // The staging tree may hold raw legacy-codepage names, which Qt cannot
    // remove on Linux; remove it byte-wise before the owner gives up.
    // 暂存目录可能含有原始旧代码页名称，Qt 在 Linux 上无法删除，
    // 因此在所有者放弃前按原始字节清理。
    FileUtils::removeDirectoryRecursivelyRaw(job.staging->path());
    job.staging->remove();
    job.staging.reset();
}

void ExternalEditManager::track(const QString &localPath, const QString &baseDir,
                                const QString &entryPath,
                                const QString &archivePath,
                                ArchiveFormat format, const QString &password,
                                const QString &nameEncoding)
{
    if (!AppSettings::instance().externalEditWatch()) {
        return;
    }
    if (localPath.isEmpty() || baseDir.isEmpty() || archivePath.isEmpty()) {
        return;
    }
    const QString normalizedEntry = FileUtils::normalizeEntryPath(entryPath);
    if (normalizedEntry.isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(localPath)) {
        return;
    }

    Job job;
    job.localPath = localPath;
    job.baseDir = baseDir;
    job.entryPath = normalizedEntry;
    job.archivePath = archivePath;
    job.password = password;
    job.nameEncoding = isUtf8NameEncoding(nameEncoding)
        ? QStringLiteral("UTF-8")
        : nameEncoding;
    job.format = format;
    adoptBaseline(job);

    m_jobs.insert(localPath, job);
    LOG_DEBUG("ExternalEditManager",
              "Watching %s (member %s of %s)",
              localPath.toUtf8().constData(),
              normalizedEntry.toUtf8().constData(),
              archivePath.toUtf8().constData());

    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

void ExternalEditManager::forget(const QString &localPath)
{
    auto it = m_jobs.find(localPath);
    if (it == m_jobs.end()) {
        return;
    }
    releaseStaging(*it);
    m_jobs.erase(it);
}

void ExternalEditManager::confirmSaved(const QString &localPath)
{
    auto it = m_jobs.find(localPath);
    if (it == m_jobs.end()) {
        return;
    }
    releaseStaging(*it);
    adoptBaseline(*it);
    it->state = Job::State::Watching;
}

void ExternalEditManager::reportSaveFailed(const QString &localPath)
{
    auto it = m_jobs.find(localPath);
    if (it == m_jobs.end()) {
        return;
    }
    releaseStaging(*it);
    adoptBaseline(*it);
    it->state = Job::State::Watching;
}

void ExternalEditManager::checkNow()
{
    if (m_jobs.isEmpty()) {
        m_timer->stop();
        return;
    }

    // Collect first: asking the user runs a nested event loop that may modify
    // the job hash (the handler can call forget()/confirmSaved()).
    // 先收集，再询问用户：询问会运行嵌套事件循环，处理函数可能修改 job 表
    //（例如调用 forget()/confirmSaved()）。
    QStringList changed;
    for (auto it = m_jobs.cbegin(); it != m_jobs.cend(); ++it) {
        if (it->state != Job::State::Watching) {
            continue;
        }
        const QFileInfo info(it->localPath);
        if (!info.exists()) {
            // Deleted or replaced by the editor: wait for it to reappear.
            // 被编辑器删除或替换：等待它重新出现。
            continue;
        }
        const qint64 size = info.size();
        const QDateTime mtime = info.lastModified();
        if (size == it->size && mtime == it->mtime) {
            continue;
        }
        changed << it.key();
    }

    for (const QString &localPath : changed) {
        auto it = m_jobs.find(localPath);
        if (it == m_jobs.end() || it->state != Job::State::Watching) {
            continue;
        }
        const QFileInfo info(localPath);
        const qint64 size = info.size();
        const QDateTime mtime = info.lastModified();

        // An editor rewrote the file without changing its content: adopt the
        // new timestamps silently instead of asking the user.
        // 编辑器重写了文件但内容未变：静默采用新的时间戳，不询问用户。
        if (!it->contentHash.isEmpty()
            && it->contentHash == hashFile(localPath, size)) {
            adoptBaseline(*it);
            continue;
        }

        if (it->pendingSize != size || it->pendingMtime != mtime) {
            // First sighting of this change: start the settle delay so a
            // multi-chunk write is not mistaken for a finished save.
            // 首次看到该更改：开始稳定延迟，避免把分块写入当成保存完成。
            it->pendingSize = size;
            it->pendingMtime = mtime;
            it->pendingSinceMs = QDateTime::currentMSecsSinceEpoch();
            continue;
        }
        if (QDateTime::currentMSecsSinceEpoch() - it->pendingSinceMs
            < m_settleDelayMs) {
            continue;
        }
        askUser(localPath);
    }
}

void ExternalEditManager::askUser(const QString &localPath)
{
    auto it = m_jobs.find(localPath);
    if (it == m_jobs.end()) {
        return;
    }
    it->state = Job::State::Asking;

    // The prompt runs a nested event loop that may add or remove jobs, so the
    // handler receives a copy and the job is looked up again afterwards.
    // 询问会运行嵌套事件循环，可能增删 job，因此处理函数收到副本，
    // 询问结束后重新查找该 job。
    const Job snapshot = *it;

    Decision decision = Decision::Discard;
    if (m_promptHandler) {
        decision = m_promptHandler(snapshot);
    }

    it = m_jobs.find(localPath);
    if (it == m_jobs.end()) {
        return;
    }
    if (decision == Decision::Discard) {
        adoptBaseline(*it);
        it->state = Job::State::Watching;
        return;
    }

    AddFilesOptions options;
    if (!prepareSave(*it, &options) || !m_saveHandler) {
        adoptBaseline(*it);
        it->state = Job::State::Watching;
        return;
    }

    it->state = Job::State::Saving;
    const Job handedOff = *it;
    m_saveHandler(handedOff, options);
}

bool ExternalEditManager::prepareSave(Job &job, AddFilesOptions *options)
{
    QString sourceRoot = job.baseDir;
    if (!isUtf8NameEncoding(job.nameEncoding)) {
        // Legacy member names need a staging tree with raw-byte names.
        // 旧式成员名需要带原始字节名称的暂存目录树。
        if (!job.staging) {
            auto staging = std::make_shared<QTemporaryDir>(
                QDir(QDir::tempPath())
                    .filePath(QStringLiteral("qarchiver-edit-XXXXXX")));
            if (!staging->isValid()) {
                LOG_WARN("ExternalEditManager",
                         "Cannot create a staging directory for %s",
                         job.entryPath.toUtf8().constData());
                return false;
            }
            job.staging = staging;
        }
        QString error;
        if (!FileUtils::stageFileForArchiving(job.localPath, job.staging->path(),
                                              job.entryPath, job.nameEncoding,
                                              &error)) {
            LOG_WARN("ExternalEditManager", "Staging failed for %s: %s",
                     job.entryPath.toUtf8().constData(),
                     error.toUtf8().constData());
            return false;
        }
        sourceRoot = job.staging->path();
    }

    options->archivePath = job.archivePath;
    options->sourceFiles = QStringList{job.entryPath};
    options->sourceRoot = sourceRoot;
    options->nameEncoding = job.nameEncoding;
    options->password = job.password;
    options->compressionLevel = AppSettings::instance().defaultCompressionLevel();
    // A single edited file: never follow symlinks, never store full paths.
    // 单个被编辑的文件：不跟随符号链接，也不保存完整路径。
    options->followSymlinks = false;
    options->storeFullPath = false;
    return true;
}
