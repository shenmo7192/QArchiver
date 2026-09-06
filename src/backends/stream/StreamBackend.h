#ifndef STREAMBACKEND_H
#define STREAMBACKEND_H

#include "backends/ArchiveBackend.h"
#include "utils/ProcessHelper.h"

// Read-only backend for pure .gz/.bz2/.xz streams (design doc 5.6.3)
class StreamBackend : public ArchiveBackend {
    Q_OBJECT
public:
    explicit StreamBackend(ArchiveFormat fmt = ArchiveFormat::Gzip, QObject *parent = nullptr);

    void openArchive(const QString &filePath) override;
    void listContents(const ListOptions &options = {}) override;
    void extract(const ExtractOptions &options) override;
    void compress(const CompressOptions &options) override;
    void addFiles(const AddFilesOptions &options) override;
    void deleteFiles(const DeleteFilesOptions &options) override;
    void testArchive(const TestOptions &options = {}) override;
    void closeArchive() override;

    ArchiveFormat format() const override { return m_format; }
    bool isReadOnly() const override { return true; }
    bool supportsEncryption() const override { return false; }
    bool supportsVolume() const override { return false; }
    bool supportsAppend() const override { return false; }

private:
    QString sevenZipPath() const;
    // Infer the internal filename from the archive path (strip last compression suffix)
    QString inferInternalName() const;

    ArchiveFormat m_format;
    ProcessHelper *m_process = nullptr;
    ArchiveEntry m_entry; // single entry
};

#endif // STREAMBACKEND_H
