#ifndef RARBACKEND_H
#define RARBACKEND_H

#include "backends/ArchiveBackend.h"
#include "utils/ProcessHelper.h"

// Read-only backend for .rar (design doc 4.2/5.4)
// Uses unrar (RARLAB official) for all operations.
class RarBackend : public ArchiveBackend {
    Q_OBJECT
public:
    explicit RarBackend(ArchiveFormat fmt = ArchiveFormat::Rar5, QObject *parent = nullptr);

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
    bool supportsEncryption() const override { return true; }
    bool supportsVolume() const override { return true; }
    bool supportsAppend() const override { return false; }

private:
    QString unrarPath() const;
    // Resolve first volume for split archives
    QString resolveFirstVolume() const;
    // Parse unrar lt output
    QList<ArchiveEntry> parseList(const QString &output) const;

    ArchiveFormat m_format;
    ProcessHelper *m_process = nullptr;
    QList<ArchiveEntry> m_entries;
    bool m_encrypted = false;
};

#endif // RARBACKEND_H
