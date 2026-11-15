#ifndef SEVENZIPBACKEND_H
#define SEVENZIPBACKEND_H

#include "backends/ArchiveBackend.h"

// Shared backend for .7z and .zip (both handled through the 7z CLI).
class SevenZipBackend : public ArchiveBackend {
    Q_OBJECT
public:
    explicit SevenZipBackend(ArchiveFormat fmt = ArchiveFormat::SevenZip,
                             QObject *parent = nullptr);

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
    bool supportsEncryption() const override { return true; }
    bool supportsVolume() const override { return true; }
    bool supportsAppend() const override;

protected:
    QString sevenZipPath() const;
    QString formatSwitch() const;
    bool isSplitArchive() const;
    QString targetPath() const;
    QList<ArchiveEntry> parseList(const QString &output) const;
    int validateLevel(int level) const;
    void appendPassword(QStringList &args, const QString &password) const;

    ArchiveFormat m_format;
    QList<ArchiveEntry> m_entries;
    bool m_encrypted = false;
};

#endif // SEVENZIPBACKEND_H
