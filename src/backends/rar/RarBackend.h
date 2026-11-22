#ifndef RARBACKEND_H
#define RARBACKEND_H

// QArchiver / 小Q解压
// RAR4/RAR5 backend: reads via bundled 7-Zip, optional creation via system rar.
// RAR4/RAR5 后端：读取使用内置 7-Zip，可选通过系统 rar 创建。

#include "backends/sevenzip/SevenZipBackend.h"

// Backend for .rar (design doc 4.2/5.4).
// RAR/RAR5 reading is delegated to the bundled full 7-Zip helper.  Creating
// archives (including .partN.rar split volumes) additionally needs the
// non-free `rar` command installed on the system; it is detected at runtime.
// .rar 后端（设计文档 4.2/5.4）。
// RAR/RAR5 读取委托给内置完整版 7-Zip。创建压缩包（包括 .partN.rar 分卷）
// 还需要系统安装非自由的 `rar` 命令，并在运行时检测。
class RarBackend : public SevenZipBackend {
    Q_OBJECT
public:
    explicit RarBackend(ArchiveFormat fmt = ArchiveFormat::Rar5,
                        QObject *parent = nullptr);

    void compress(const CompressOptions &options) override;

    // Existing archives stay read-only; compress() can create a new one.
    // 已有压缩包保持只读；compress() 可以创建新压缩包。
    bool isReadOnly() const override { return true; }
    bool supportsAppend() const override { return false; }

    // Whether the system `rar` command can be used to create archives.
    // 是否可以使用系统 `rar` 命令创建压缩包。
    static bool isCreateAvailable();
    // Resolved `rar` executable (empty when unavailable).
    // 解析后的 `rar` 可执行文件路径（不可用时为空）。
    static QString createToolPath();

private:
    QString rarPath() const;
};

#endif // RARBACKEND_H
