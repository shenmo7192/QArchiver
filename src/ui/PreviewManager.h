#ifndef PREVIEWMANAGER_H
#define PREVIEWMANAGER_H

#include <QObject>
#include <QString>

#include "core/ArchiveConstants.h"

// Manages temporary extraction for file preview (design doc 5.6).
// Preview extraction never touches the main backend: it creates a private
// backend inside a worker thread, extracts one entry, then destroys it.
class PreviewManager : public QObject {
    Q_OBJECT
public:
    static PreviewManager &instance();

    void preview(const QString &archivePath, ArchiveFormat format,
                 const QString &entryPath, qint64 entrySize,
                 const QString &password);

    // Cleanup stale preview directories (called at startup)
    void cleanupStale();

    // Cleanup this process's preview directory (called at exit)
    void cleanupOwn();

private:
    PreviewManager();
    Q_DISABLE_COPY(PreviewManager)

    QString previewRoot() const;
    QString ownPreviewDir() const;
};

#endif // PREVIEWMANAGER_H
