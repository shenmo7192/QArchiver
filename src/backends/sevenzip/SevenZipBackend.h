#ifndef SEVENZIPBACKEND_H
#define SEVENZIPBACKEND_H

#include "backends/ArchiveBackend.h"
#include "utils/ProcessHelper.h"

// Backend for .7z (design doc 4.2/5.1/5.2)
class SevenZipBackend : public ArchiveBackend {
    Q_OBJECT
public:
    explicit SevenZipBackend(QObject *parent = nullptr);

    void openArchive(const QString &filePath) override;
    void listContents(const ListOptions &options = {}) override;
    void extract(const ExtractOptions &options) override;
    void compress(const CompressOptions &options) override;
    void addFiles(const AddFilesOptions &options) override;
    void deleteFiles(const DeleteFilesOptions &options) override;
    void testArchive(const TestOptions &options = {}) override;
    void closeArchive() override;

    ArchiveFormat format() const override { return ArchiveFormat::SevenZip; }
    bool isReadOnly() const override { return false; }
    bool supportsEncryption() const override { return true; }
    bool supportsVolume() const override { return true; }
    bool supportsAppend() const override;

private:
    QString sevenZipPath() const;
    bool isSplitArchive() const;
    QList<ArchiveEntry> parseList(const QString &output) const;
    int validateLevel(int level) const;

    ProcessHelper *m_process = nullptr;
    QList<ArchiveEntry> m_entries;
    bool m_encrypted = false;
};

#endif // SEVENZIPBACKEND_H
