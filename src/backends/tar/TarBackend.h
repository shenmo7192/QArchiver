#ifndef TARBACKEND_H
#define TARBACKEND_H

// QArchiver / 小Q解压
// Tar family backend implemented with the bundled 7-Zip helper.
// 使用内置 7-Zip 辅助程序实现的 tar 系列后端。

#include "backends/ArchiveBackend.h"

// Backend for .tar, .tar.gz/.tgz, .tar.xz/.txz, .tar.bz2/.tbz2 (design doc 4.2/5.1/5.2).
// All tar operations are delegated to the bundled 7-Zip helper.  Compressed
// tar files are single-stream formats, so creation/extraction/listing happens
// in two stages through a temporary uncompressed .tar.
// .tar、.tar.gz/.tgz、.tar.xz/.txz、.tar.bz2/.tbz2 后端（设计文档 4.2/5.1/5.2）。
// 所有 tar 操作都委托给内置 7-Zip 辅助程序。压缩 tar 是单流格式，
// 因此创建/解压/列表都通过临时未压缩 .tar 分两阶段完成。
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
    enum class InnerTarResult {
        Ok,
        Cancelled,
        Failed
    };

    bool isCompressed() const;
    QString compressionSwitch() const;
    // Validate and normalize compression level; return -1 to omit
    // 校验并规范化压缩级别；返回 -1 表示省略该参数
    int validateCompressionLevel(int level) const;
    // Extract the single .tar member from a compressed tar stream into
    // @p outerDir.  Returns the path of the inner tar on success.
    // 从压缩 tar 流中把唯一的 .tar 成员解压到 @p outerDir；
    // 成功时返回内部 tar 的路径。
    InnerTarResult extractInnerTar(const QString &sevenZip,
                                   const QString &outerDir,
                                   QString &innerTarPath,
                                   QString &errorText);
    ArchiveFormat m_format;
    QList<ArchiveEntry> m_entries;   // Cached entry list for progress calculation / 缓存的条目列表，用于进度计算
    QString m_nameEncoding = QStringLiteral("UTF-8");
};

#endif // TARBACKEND_H
