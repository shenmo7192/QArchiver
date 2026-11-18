#ifndef STREAMBACKEND_H
#define STREAMBACKEND_H

// QArchiver / 小Q解压
// Backend for single-stream gzip, bzip2 and xz files.
// 用于单流 gzip、bzip2 与 xz 文件的后端。

#include "backends/ArchiveBackend.h"

// Read-only backend for pure .gz/.bz2/.xz streams (design doc 5.6.3).
// 纯 .gz/.bz2/.xz 流的只读后端（设计文档 5.6.3）。
class StreamBackend : public ArchiveBackend {
    Q_OBJECT
public:
    explicit StreamBackend(ArchiveFormat fmt = ArchiveFormat::Gzip,
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
    bool isReadOnly() const override { return true; }
    bool supportsEncryption() const override { return false; }
    bool supportsVolume() const override { return false; }
    bool supportsAppend() const override { return false; }

private:
    QString sevenZipPath() const;
    QString inferInternalName() const;
    qint64 queryUncompressedSize() const;

    ArchiveFormat m_format;
    ArchiveEntry m_entry;
};

#endif // STREAMBACKEND_H
