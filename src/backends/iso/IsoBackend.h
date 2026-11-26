#ifndef ISOBACKEND_H
#define ISOBACKEND_H

// QArchiver / 小Q解压
// Read-only disc image (.iso) backend built on the bundled 7-Zip helper.
// 基于内置 7-Zip 辅助程序的只读光盘镜像（.iso）后端。

#include "backends/sevenzip/SevenZipBackend.h"

// Read-only backend for .iso disc images (ISO9660/Joliet/Rock Ridge and UDF).
//
// 7-Zip lists and extracts ISO images with the same commands used for 7z/zip,
// so only the write paths have to be disabled: ISO9660/UDF have no writer in
// 7-Zip and the format is not part of the QArchiver creation dialog.
// Reading prefers the in-process libarchive backend (ISO9660 with Rock Ridge
// and Joliet); UDF images are handled by the automatic libarchive -> CLI
// fallback in ArchiveManager.
//
// 只读的 .iso 光盘镜像后端（ISO9660/Joliet/Rock Ridge 与 UDF）。
//
// 7-Zip 用与 7z/zip 相同的命令列出和解压 ISO 镜像，因此只需禁用写入路径：
// 7-Zip 没有 ISO9660/UDF 写入器，该格式也不在 QArchiver 的创建对话框中。
// 读取优先使用进程内 libarchive 后端（支持 Rock Ridge 与 Joliet 的 ISO9660）；
// UDF 镜像由 ArchiveManager 的 libarchive -> CLI 自动回退处理。
class IsoBackend : public SevenZipBackend {
    Q_OBJECT
public:
    explicit IsoBackend(QObject *parent = nullptr)
        : SevenZipBackend(ArchiveFormat::Iso, parent)
    {
    }

    void compress(const CompressOptions &options) override;
    void addFiles(const AddFilesOptions &options) override;
    void deleteFiles(const DeleteFilesOptions &options) override;

    ArchiveFormat format() const override { return ArchiveFormat::Iso; }
    // Disc images cannot be modified in place. / 光盘镜像无法就地修改。
    bool isReadOnly() const override { return true; }
    bool supportsEncryption() const override { return false; }
    bool supportsVolume() const override { return false; }
    bool supportsAppend() const override { return false; }
};

#endif // ISOBACKEND_H
