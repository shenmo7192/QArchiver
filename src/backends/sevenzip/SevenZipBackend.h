#ifndef SEVENZIPBACKEND_H
#define SEVENZIPBACKEND_H

// QArchiver / 小Q解压
// Shared write-capable backend for 7z and zip archives.
// 支持写入的 7z 与 zip 共用后端。

#include "backends/ArchiveBackend.h"

// Shared backend for .7z and .zip (both handled through the 7z CLI).
// .7z 与 .zip 的共用后端（均通过 7z 命令行处理）。
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
    QString nameEncoding() const override { return m_nameEncoding; }
    bool isReadOnly() const override { return false; }
    bool supportsEncryption() const override { return true; }
    bool supportsVolume() const override { return true; }
    bool supportsAppend() const override;

protected:
    QString sevenZipPath() const;
    QString formatSwitch() const;
    bool isSplitArchive() const;
    QString targetPath() const;
    int validateLevel(int level) const;
    void appendPassword(QStringList &args, const QString &password) const;

    ArchiveFormat m_format;
    QList<ArchiveEntry> m_entries;
    bool m_encrypted = false;
    // Encoding of the archive's member names, detected from the raw `l -slt`
    // output.  Used to build a listfile for selected extraction/deletion when
    // a legacy ZIP stores non-UTF-8 names.
    // 压缩包成员名称的编码，从 `l -slt` 原始输出中检测。
    // 用于在旧式 ZIP 保存非 UTF-8 名称时构建选定解压/删除操作的列表文件。
    QString m_nameEncoding = QStringLiteral("UTF-8");
};

#endif // SEVENZIPBACKEND_H
