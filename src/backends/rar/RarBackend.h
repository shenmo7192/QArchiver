#ifndef RARBACKEND_H
#define RARBACKEND_H

// QArchiver / 小Q解压
// Read-only RAR4/RAR5 backend delegated to the bundled 7-Zip helper.
// 委托给内置 7-Zip 辅助程序的只读 RAR4/RAR5 后端。

#include "backends/sevenzip/SevenZipBackend.h"

// Read-only backend for .rar (design doc 4.2/5.4).
// RAR/RAR5 reading is delegated to the bundled full 7-Zip helper; the 7z
// backend already implements the common list/extract/test/volume logic.
// .rar 只读后端（设计文档 4.2/5.4）。
// RAR/RAR5 读取委托给内置完整版 7-Zip；7z 后端已实现通用的
// 列表、解压、测试与分卷逻辑。
class RarBackend : public SevenZipBackend {
    Q_OBJECT
public:
    explicit RarBackend(ArchiveFormat fmt = ArchiveFormat::Rar5,
                        QObject *parent = nullptr);

    bool isReadOnly() const override { return true; }
    bool supportsAppend() const override { return false; }
};

#endif // RARBACKEND_H
