// QArchiver / 小Q解压
// Thumbnail extraction/cache implementation.
// 缩略图解压与缓存实现。

#include "ThumbnailManager.h"

#include "backends/ArchiveBackend.h"
#include "core/ArchiveBackendFactory.h"
#include "common/ArchiveOptions.h"
#include "ui/icons/IconUtils.h"
#include "utils/FileUtils.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMetaObject>
#include <QPainter>
#include <QPixmap>
#include <QRect>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtConcurrent>

#include <memory>

namespace {
constexpr int kThumbnailSize = 128;
constexpr int kMaxSourceSize = 512;

QString cacheRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QStringLiteral("thumbnails"));
}

QString archiveIdentity(const QString &archivePath)
{
    const QFileInfo info(archivePath);
    return archivePath + QLatin1Char('|')
        + QString::number(info.size()) + QLatin1Char('|')
        + QString::number(info.lastModified().toMSecsSinceEpoch());
}

QString thumbnailKey(const QString &archivePath, const QString &entryPath,
                     qint64 entrySize)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(
            (archiveIdentity(archivePath) + QLatin1Char('|') + entryPath
             + QLatin1Char('|') + QString::number(entrySize)).toUtf8(),
            QCryptographicHash::Md5).toHex());
}
} // namespace

ThumbnailManager::ThumbnailManager(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(2);
}

ThumbnailManager &ThumbnailManager::instance()
{
    static ThumbnailManager manager;
    return manager;
}

QIcon ThumbnailManager::icon(const QString &archivePath, ArchiveFormat format,
                             const QString &entryPath, qint64 entrySize,
                             const QString &password)
{
    if (archivePath.isEmpty() || entryPath.isEmpty()) {
        return Icons::fileType(QStringLiteral("file-image.png"));
    }

    const QString key = thumbnailKey(archivePath, entryPath, entrySize);
    if (const auto it = m_icons.constFind(key); it != m_icons.constEnd()) {
        return it.value();
    }
    if (m_failed.contains(key)) {
        return Icons::fileType(QStringLiteral("file-image.png"));
    }

    const QString diskFile = QDir(cacheRoot()).filePath(key
                                                        + QStringLiteral(".png"));
    if (QFileInfo::exists(diskFile)) {
        QPixmap pixmap;
        if (pixmap.load(diskFile)) {
            const QIcon icon(pixmap);
            m_icons.insert(key, icon);
            return icon;
        }
    }

    if (!m_pending.contains(key)) {
        m_pending.insert(key);
        requestThumbnail(key, diskFile, archivePath, entryPath, entrySize,
                         format, password);
    }
    return Icons::fileType(QStringLiteral("file-image.png"));
}

void ThumbnailManager::requestThumbnail(const QString &key,
                                        const QString &cacheFile,
                                        const QString &archivePath,
                                        const QString &entryPath,
                                        qint64 entrySize,
                                        ArchiveFormat format,
                                        const QString &password)
{
    Q_UNUSED(entrySize);
    const QString normalized = FileUtils::normalizeEntryPath(entryPath);
    if (normalized.isEmpty()) {
        m_pending.remove(key);
        m_failed.insert(key);
        return;
    }

    QtConcurrent::run(&m_pool, [this, key, cacheFile, archivePath, entryPath,
                                normalized, format, password]() {
        bool success = false;
        bool backendSuccess = false;
        QString errorMessage;

        QDir().mkpath(QFileInfo(cacheFile).absolutePath());
        QTemporaryDir tempDir(
            QDir(cacheRoot()).filePath(QStringLiteral("extract-XXXXXX")));
        if (tempDir.isValid()) {
            std::unique_ptr<ArchiveBackend> backend(
                ArchiveBackendFactory::createForRead(format, archivePath));
            if (backend) {
                QObject::connect(
                    backend.get(), &ArchiveBackend::operationFinished,
                    backend.get(),
                    [&backendSuccess, &errorMessage](bool ok,
                                                     const QString &message) {
                        backendSuccess = ok;
                        if (!ok) {
                            errorMessage = message;
                        }
                    },
                    Qt::DirectConnection);
                QObject::connect(
                    backend.get(), &ArchiveBackend::errorOccurred,
                    backend.get(),
                    [&backendSuccess, &errorMessage](ArchiveErrorCode,
                                                     const QString &message) {
                        backendSuccess = false;
                        errorMessage = message;
                    },
                    Qt::DirectConnection);

                backend->openArchive(archivePath);
                ExtractOptions options;
                options.destDir = tempDir.path();
                options.selectedFiles = {entryPath};
                options.preservePaths = true;
                options.overwrite = OverwritePolicy::Always;
                options.password = password;
                backend->extract(options);
                backend->closeArchive();
            }
        }

        if (tempDir.isValid() && backendSuccess) {
            QString extracted = QDir(tempDir.path()).filePath(normalized);
            if (!QFileInfo::exists(extracted)) {
                const QString baseName =
                    QFileInfo(normalized).fileName();
                QDirIterator it(tempDir.path(), QDir::Files,
                                QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    const QString candidate = it.next();
                    if (QFileInfo(candidate).fileName() == baseName) {
                        extracted = candidate;
                        break;
                    }
                }
            }
            if (QFileInfo::exists(extracted)) {
                QImageReader reader(extracted);
                reader.setAutoTransform(true);
                const QSize sourceSize = reader.size();
                if (sourceSize.isValid()
                    && (sourceSize.width() > kMaxSourceSize
                        || sourceSize.height() > kMaxSourceSize)) {
                    reader.setScaledSize(sourceSize.scaled(
                        kMaxSourceSize, kMaxSourceSize,
                        Qt::KeepAspectRatio));
                }
                const QImage image = reader.read();
                if (!image.isNull()) {
                    const QImage scaled = image.scaled(
                        kThumbnailSize, kThumbnailSize,
                        Qt::KeepAspectRatioByExpanding,
                        Qt::SmoothTransformation);
                    QImage square(kThumbnailSize, kThumbnailSize,
                                  QImage::Format_ARGB32_Premultiplied);
                    square.fill(Qt::transparent);
                    QPainter painter(&square);
                    painter.drawImage(
                        QRect(0, 0, kThumbnailSize, kThumbnailSize), scaled,
                        QRect((scaled.width() - kThumbnailSize) / 2,
                              (scaled.height() - kThumbnailSize) / 2,
                              kThumbnailSize, kThumbnailSize));
                    painter.end();

                    const QString temporary = cacheFile
                        + QStringLiteral(".tmp");
                    if (square.save(temporary, "PNG")) {
                        QFile::remove(cacheFile);
                        success = QFile::rename(temporary, cacheFile);
                    }
                }
            }
        }
        Q_UNUSED(errorMessage);

        QMetaObject::invokeMethod(
            this,
            [this, key, cacheFile, archivePath, entryPath, success]() {
                m_pending.remove(key);
                if (success) {
                    QPixmap pixmap;
                    if (pixmap.load(cacheFile)) {
                        m_icons.insert(key, QIcon(pixmap));
                        emit thumbnailReady(archivePath, entryPath);
                    } else {
                        m_failed.insert(key);
                    }
                } else {
                    m_failed.insert(key);
                }
            },
            Qt::QueuedConnection);
    });
}

void ThumbnailManager::clearFailures()
{
    m_failed.clear();
}
