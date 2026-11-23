#ifndef THUMBNAILMANAGER_H
#define THUMBNAILMANAGER_H

// QArchiver / 小Q解压
// Lazy image thumbnails for entries inside archives.
// 压缩包内图片的按需缩略图缓存。

#include <QHash>
#include <QIcon>
#include <QObject>
#include <QSet>
#include <QThreadPool>

#include "common/ArchiveConstants.h"

// Archive entries are not real files.  Thumbnails are extracted into a
// persistent cache by a small worker pool, then exposed to FileListModel.
// 压缩包条目不是真实文件；这里用小型线程池把图片解压到持久缓存，再提供给
// FileListModel 作为图标。
class ThumbnailManager : public QObject {
    Q_OBJECT
public:
    static ThumbnailManager &instance();

    // Return a cached thumbnail immediately, or a generic image icon while
    // the extraction request runs in the background.
    // 命中缓存时立即返回缩略图；否则返回通用图片图标并在后台解压。
    QIcon icon(const QString &archivePath, ArchiveFormat format,
               const QString &entryPath, qint64 entrySize,
               const QString &password);

    // Allow previously failed entries (wrong password, unsupported image) to
    // be retried after the archive/password changes.
    // 在压缩包或密码变化后允许重试此前失败的条目。
    void clearFailures();

signals:
    void thumbnailReady(const QString &archivePath, const QString &entryPath);

private:
    explicit ThumbnailManager(QObject *parent = nullptr);

    void requestThumbnail(const QString &key, const QString &cacheFile,
                          const QString &archivePath,
                          const QString &entryPath, qint64 entrySize,
                          ArchiveFormat format, const QString &password);
    QHash<QString, QIcon> m_icons;
    QSet<QString> m_pending;
    QSet<QString> m_failed;
    QThreadPool m_pool;
};

#endif // THUMBNAILMANAGER_H
