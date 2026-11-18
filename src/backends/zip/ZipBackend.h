#ifndef ZIPBACKEND_H
#define ZIPBACKEND_H

// QArchiver / 小Q解压
// Thin zip specialization of SevenZipBackend.
// SevenZipBackend 的轻量 zip 特化。

#include "backends/sevenzip/SevenZipBackend.h"

// Thin .zip specialization: all process handling and parsing is shared with
// SevenZipBackend; only the 7z format switch (-tzip) differs.
// 轻量 .zip 特化：所有进程处理与解析都与 SevenZipBackend 共用，
// 仅 7z 格式开关（-tzip）不同。
class ZipBackend : public SevenZipBackend {
    Q_OBJECT
public:
    explicit ZipBackend(QObject *parent = nullptr)
        : SevenZipBackend(ArchiveFormat::Zip, parent)
    {
    }

    ArchiveFormat format() const override { return ArchiveFormat::Zip; }
};

#endif // ZIPBACKEND_H
