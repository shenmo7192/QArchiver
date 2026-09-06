#ifndef TARBACKEND_H
#define TARBACKEND_H

#include "backends/ArchiveBackend.h"
#include "utils/ProcessHelper.h"

// Backend for .tar, .tar.gz/.tgz, .tar.xz/.txz, .tar.bz2/.tbz2 (design doc 4.2/5.1/5.2)
class TarBackend : public ArchiveBackend {
    Q_OBJECT
public:
    explicit TarBackend(ArchiveFormat fmt = ArchiveFormat::Tar, QObject *parent = nullptr);

    void openArchive(const QString &filePath) override;
    void listContents(const ListOptions &options = {}) override;
    void extract(const ExtractOptions &options) override;
    void compress(const CompressOptions &options) override;
    void addFiles(const AddFilesOptions &options) override;
    void deleteFiles(const DeleteFilesOptions &options) override;
    void testArchive(const TestOptions &options = {}) override;
    void closeArchive() override;

    ArchiveFormat format() const override { return m_format; }
    bool isReadOnly() const override { return false; }
    bool supportsEncryption() const override { return false; }
    bool supportsVolume() const override { return false; }
    bool supportsAppend() const override;

private:
    // Determine the tar decompression flag (-z/-J/-j) for the current format
    QString decompressFlag() const;
    // Determine the tar compress flag for compression target
    QString compressFlag(ArchiveFormat fmt) const;
    // Validate and normalize compression level; return default if invalid
    int validateCompressionLevel(int level) const;
    // Build the exclude arguments
    QStringList excludeArgs(const QStringList &patterns) const;

    ArchiveFormat m_format;
    ProcessHelper *m_process = nullptr;
    QList<ArchiveEntry> m_entries;   // Cached entry list for progress calculation
    bool m_cancelled = false;
};

#endif // TARBACKEND_H
