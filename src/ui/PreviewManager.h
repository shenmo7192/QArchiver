#ifndef PREVIEWMANAGER_H
#define PREVIEWMANAGER_H

#include <QObject>
#include <QString>

class ArchiveBackend;

// Manages temporary extraction for file preview (design doc 5.6)
class PreviewManager : public QObject {
    Q_OBJECT
public:
    static PreviewManager &instance();

    // Preview a single entry from an archive
    void preview(ArchiveBackend *backend, const QString &entryPath, qint64 entrySize);

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
