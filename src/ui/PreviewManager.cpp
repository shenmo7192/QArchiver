#include "PreviewManager.h"

#include "backends/ArchiveBackend.h"
#include "core/ArchiveOptions.h"
#include "core/AppSettings.h"
#include "core/ArchiveError.h"
#include "utils/FileUtils.h"
#include "utils/Logger.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QCryptographicHash>
#include <QCoreApplication>

PreviewManager::PreviewManager()
{
}

PreviewManager &PreviewManager::instance()
{
    static PreviewManager s;
    return s;
}

QString PreviewManager::previewRoot() const
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return QDir(cacheDir).filePath(QStringLiteral("SimpleArchiveManager/preview"));
}

QString PreviewManager::ownPreviewDir() const
{
    return QDir(previewRoot()).filePath(QString::number(QCoreApplication::applicationPid()));
}

void PreviewManager::preview(ArchiveBackend *backend, const QString &entryPath, qint64 entrySize)
{
    if (!backend) return;

    // Size check
    if (entrySize > AppSettings::instance().maxPreviewSize()) {
        LOG_WARN("PreviewManager", "File too large to preview: %lld bytes", entrySize);
        return;
    }

    // Create per-process, per-archive preview dir
    QString archiveHash = QCryptographicHash::hash(
        backend->archivePath().toUtf8(), QCryptographicHash::Md5).toHex();
    QString previewDir = QDir(ownPreviewDir()).filePath(archiveHash);
    QDir().mkpath(previewDir);

    // Check if already extracted
    QString tempPath = QDir(previewDir).filePath(QFileInfo(entryPath).fileName());
    if (QFileInfo::exists(tempPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath));
        return;
    }

    // Extract single entry to temp dir
    ExtractOptions opts;
    opts.destDir = previewDir;
    opts.selectedFiles = QStringList{entryPath};
    opts.overwrite = OverwritePolicy::Always;

    // Connect to backend's operationFinished
    QObject::connect(backend, &ArchiveBackend::operationFinished,
        backend, [tempPath](bool success, const QString &) {
            if (success && QFileInfo::exists(tempPath)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(tempPath));
            }
        }, static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::UniqueConnection));

    backend->extract(opts);
}

void PreviewManager::cleanupStale()
{
    QDir root(previewRoot());
    if (!root.exists()) return;

    // Remove preview dirs for processes no longer running
    for (const QString &entry : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        qint64 pid = entry.toLongLong(&ok);
        if (ok && pid > 0) {
            // Check if /proc/<pid> exists (process running)
            if (!QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid))) {
                QDir dir(root.absoluteFilePath(entry));
                dir.removeRecursively();
                LOG_DEBUG("PreviewManager", "Cleaned stale preview dir for PID %lld", pid);
            }
        }
    }
}

void PreviewManager::cleanupOwn()
{
    QDir dir(ownPreviewDir());
    if (dir.exists()) {
        dir.removeRecursively();
    }
}
