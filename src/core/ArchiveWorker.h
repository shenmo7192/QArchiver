#ifndef ARCHIVEWORKER_H
#define ARCHIVEWORKER_H

#include <QObject>
#include <QPointer>
#include "core/ArchiveOptions.h"
#include "core/ArchiveError.h"

class ArchiveBackend;

// Wraps async archive operations via QtConcurrent (design doc 3 Core_Control)
// Catches exceptions from backends and converts them to errorOccurred signals.
class ArchiveWorker : public QObject {
    Q_OBJECT
public:
    explicit ArchiveWorker(ArchiveBackend *backend, QObject *parent = nullptr);

    void runList(const ListOptions &options);
    void runExtract(const ExtractOptions &options);
    void runCompress(const CompressOptions &options);
    void runTest(const TestOptions &options);
    void runAddFiles(const AddFilesOptions &options);
    void runDeleteFiles(const DeleteFilesOptions &options);

signals:
    void finished();
    void error(ArchiveErrorCode code, const QString &msg);

private:
    QPointer<ArchiveBackend> m_backend;
};

#endif // ARCHIVEWORKER_H
