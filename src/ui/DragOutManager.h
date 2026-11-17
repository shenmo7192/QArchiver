#ifndef DRAGOUTMANAGER_H
#define DRAGOUTMANAGER_H

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <atomic>
#include <memory>

#include "core/ArchiveConstants.h"
#include "core/ArchiveEntry.h"
#include "core/ArchiveError.h"

class ArchiveBackend;

// Result of a "prepare files for dragging" request.
struct DragOutPayload {
    QString stagingRoot;      // Per-drag staging directory (kept until exit)
    QStringList localPaths;   // Real local paths handed to QMimeData
    QString errorMessage;
    ArchiveErrorCode errorCode = ArchiveErrorCode::Unknown;
    bool success = false;
    bool cancelled = false;
};

// Prepares real local files for a drag-out gesture (design doc "drag out").
//
// Archive entries do not exist on disk, but the window manager / file manager
// only understands "text/uri-list" with real file URLs.  This class extracts
// the selected entries into a per-drag staging directory and maps the archive
// paths back to the staged files.  Staging is kept until the application
// exits because the drop target may still be copying asynchronously.
class DragOutManager : public QObject {
    Q_OBJECT
public:
    static DragOutManager &instance();

    // Synchronous: runs the extraction in a worker thread and spins a nested
    // event loop so status bar updates stay responsive.  The caller must start
    // the QDrag immediately afterwards, while the mouse button is still down.
    // Successful preparations are cached, so a second drag of the same
    // selection (and password) starts without any delay.
    DragOutPayload extractForDrag(const QString &archivePath,
                                  ArchiveFormat format,
                                  const QList<ArchiveEntry> &entries,
                                  const QStringList &selectedPaths,
                                  const QString &password,
                                  const QString &encodingHint = QString());

    // Cancel the preparation in progress (Esc while dragging).
    void cancel();

    // Keep the staging of a finished drag until the application exits.
    void retire(const DragOutPayload &payload);

    // A previously prepared staging for this archive/selection/password.  The
    // drag gesture cannot wait for a slow extraction without breaking the
    // mouse press, so a finished preparation is reused on the next attempt.
    bool hasPreparedFor(const QString &archivePath, ArchiveFormat format,
                        const QStringList &paths,
                        const QString &password) const;
    DragOutPayload takePreparedFor(const QString &archivePath,
                                   ArchiveFormat format,
                                   const QStringList &paths,
                                   const QString &password);

    // Drop cached staging (archive closed / replaced).
    void clearCache();

    // Remove staging left behind by processes that no longer exist.
    void cleanupStale();

    // Remove this process' staging (called on normal shutdown).
    void cleanupOwn();

    // Helpers, exposed as public statics so they can be unit tested.
    static QStringList pruneNestedPaths(const QStringList &paths);
    static QStringList mapStagedPaths(const QString &stagingRoot,
                                      const QStringList &selectedPaths);
    static QString findStagedPathFallback(const QString &stagingRoot,
                                          const QString &normalized);
    // Top-level files of a staging directory; used for plain stream formats
    // (.gz/.bz2/.xz), where the backend ignores selectedFiles.
    static QStringList topLevelStagedPaths(const QString &stagingRoot);
    static qint64 selectedUncompressedSize(const QList<ArchiveEntry> &entries,
                                           const QStringList &paths);
    // Only errors that the command line tools can plausibly fix are retried.
    static bool isRetryableWithCli(ArchiveErrorCode code);

signals:
    void progressChanged(int percent, int total, int current, int etaSec);
    void stageMessage(const QString &message);

private:
    DragOutManager();
    Q_DISABLE_COPY(DragOutManager)

    struct ExtractionContext {
        std::atomic_bool cancelRequested{false};
        std::atomic<ArchiveBackend *> backend{nullptr};
    };

    QString dragRoot() const;
    QString createStagingRoot() const;

    static QString cacheKey(const QString &archivePath, ArchiveFormat format,
                            const QStringList &paths, const QString &password);
    void storePrepared(const QString &key, const DragOutPayload &payload);

    std::shared_ptr<ExtractionContext> m_context;
    QSet<QString> m_retainedRoots;

    struct CacheEntry {
        QString stagingRoot;
        QStringList localPaths;
    };
    QHash<QString, CacheEntry> m_cache;  // most recent last; size is capped
};

#endif // DRAGOUTMANAGER_H
