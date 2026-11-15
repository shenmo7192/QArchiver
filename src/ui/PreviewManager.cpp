#include "PreviewManager.h"

#include "backends/ArchiveBackend.h"
#include "core/ArchiveManager.h"
#include "core/ArchiveOptions.h"
#include "core/AppSettings.h"
#include "core/ArchiveError.h"
#include "utils/FileUtils.h"
#include "utils/Logger.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

#include <memory>

PreviewManager::PreviewManager()
{
}

PreviewManager &PreviewManager::instance()
{
    static PreviewManager manager;
    return manager;
}

QString PreviewManager::previewRoot() const
{
    const QString cacheDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return QDir(cacheDir).filePath(QStringLiteral("preview"));
}

QString PreviewManager::ownPreviewDir() const
{
    return QDir(previewRoot()).filePath(
        QString::number(QCoreApplication::applicationPid()));
}

void PreviewManager::preview(const QString &archivePath, ArchiveFormat format,
                             const QString &entryPath, qint64 entrySize,
                             const QString &password)
{
    if (archivePath.isEmpty() || entryPath.isEmpty()) {
        return;
    }
    if (entrySize > AppSettings::instance().maxPreviewSize()) {
        QMessageBox::information(
            nullptr, tr("Preview"),
            tr("This file is too large to preview (%1).")
                .arg(QLocale().formattedDataSize(entrySize)));
        return;
    }

    const QString archiveHash = QCryptographicHash::hash(
        archivePath.toUtf8(), QCryptographicHash::Md5).toHex();
    const QString previewDir = QDir(ownPreviewDir()).filePath(archiveHash);
    const QString normalized = FileUtils::normalizeEntryPath(entryPath);

    if (FileUtils::isPathTraversal(normalized, previewDir)) {
        LOG_WARN("PreviewManager", "Blocked unsafe preview path: %s",
                 entryPath.toUtf8().constData());
        return;
    }

    const QString destPath = QDir(previewDir).filePath(normalized);
    if (QFileInfo::exists(destPath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(destPath));
        return;
    }

    QDir().mkpath(QFileInfo(destPath).absolutePath());

    // Do not call the main backend from the GUI thread.  A private backend is
    // created, used and destroyed entirely inside the worker thread.
    (void)QtConcurrent::run([archivePath, format, entryPath, normalized, previewDir,
                              destPath, password]() {
        std::unique_ptr<ArchiveBackend> backend(
            ArchiveManager::createBackend(format));

        bool success = false;
        QString errorMessage;

        if (backend) {
            QObject::connect(
                backend.get(), &ArchiveBackend::operationFinished,
                backend.get(),
                [&success, &errorMessage](bool ok, const QString &msg) {
                    success = ok;
                    errorMessage = msg;
                },
                Qt::DirectConnection);
            QObject::connect(
                backend.get(), &ArchiveBackend::errorOccurred,
                backend.get(),
                [&success, &errorMessage](ArchiveErrorCode, const QString &msg) {
                    success = false;
                    errorMessage = msg;
                },
                Qt::DirectConnection);

            backend->openArchive(archivePath);

            ExtractOptions options;
            options.destDir = previewDir;
            options.selectedFiles = QStringList{entryPath};
            options.overwrite = OverwritePolicy::Always;
            options.preservePaths = true;
            options.password = password;

            backend->extract(options);
            backend->closeArchive();
        } else {
            errorMessage = QObject::tr("No backend available for preview");
        }

        const bool fileExists = QFileInfo::exists(destPath);

        QMetaObject::invokeMethod(
            &PreviewManager::instance(),
            [destPath, success, fileExists, errorMessage]() {
                if (success && fileExists) {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(destPath));
                    return;
                }
                LOG_WARN("PreviewManager", "Preview failed: %s",
                         errorMessage.toUtf8().constData());
                QMessageBox::warning(
                    nullptr, QObject::tr("Preview"),
                    errorMessage.isEmpty()
                        ? QObject::tr("Unable to extract this file for preview.")
                        : errorMessage);
            },
            Qt::QueuedConnection);
    });
}

void PreviewManager::cleanupStale()
{
    QDir root(previewRoot());
    if (!root.exists()) {
        return;
    }

    const QStringList entries = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        bool ok = false;
        const qint64 pid = entry.toLongLong(&ok);
        if (ok && pid > 0
            && !QFileInfo::exists(QStringLiteral("/proc/%1").arg(pid))) {
            QDir(root.absoluteFilePath(entry)).removeRecursively();
            LOG_DEBUG("PreviewManager", "Removed stale preview dir for PID %lld", pid);
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
