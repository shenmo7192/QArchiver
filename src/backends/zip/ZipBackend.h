#ifndef ZIPBACKEND_H
#define ZIPBACKEND_H

#include "backends/ArchiveBackend.h"
#include "utils/ProcessHelper.h"

// Backend for .zip (handled via 7z, design doc 4.2/5.1/5.2)
class ZipBackend : public ArchiveBackend {
    Q_OBJECT
public:
    explicit ZipBackend(QObject *parent = nullptr);

    void openArchive(const QString &filePath) override;
    void listContents(const ListOptions &options = {}) override;
    void extract(const ExtractOptions &options) override;
    void compress(const CompressOptions &options) override;
    void addFiles(const AddFilesOptions &options) override;
    void deleteFiles(const DeleteFilesOptions &options) override;
    void testArchive(const TestOptions &options = {}) override;
    void closeArchive() override;

    ArchiveFormat format() const override { return ArchiveFormat::Zip; }
    bool isReadOnly() const override { return false; }
    bool supportsEncryption() const override { return true; }
    bool supportsVolume() const override { return true; }
    bool supportsAppend() const override;

private:
    // Resolve the 7z tool path
    QString sevenZipPath() const;
    // Check if the current archive is a split volume
    bool isSplitArchive() const;
    // Parse 7z list output (lt format)
    QList<ArchiveEntry> parseList(const QString &output) const;
    // Validate compression level (0-9)
    int validateLevel(int level) const;

    ProcessHelper *m_process = nullptr;
    QList<ArchiveEntry> m_entries;
    bool m_encrypted = false;
};

#endif // ZIPBACKEND_H
