#ifndef QARCHIVER_CORE_ARCHIVEBACKENDFACTORY_H
#define QARCHIVER_CORE_ARCHIVEBACKENDFACTORY_H

// QArchiver / 小Q解压
// Backend factory and libarchive dispatch policy.
// 后端工厂与 libarchive 调度策略。

#include <QString>

#include "common/ArchiveConstants.h"

class ArchiveBackend;

// Creates archive backends and decides which read path serves an archive.
// Extracted from ArchiveManager so the policy can be used (and tested) without
// the asynchronous controller and its worker thread.
// 创建压缩包后端并决定某压缩包走哪条读取路径。从 ArchiveManager 中抽出，
// 使该策略无需异步控制器及其工作线程即可使用（并被测试）。
class ArchiveBackendFactory {
public:
    // Backend for write operations and for formats without a libarchive path.
    // Always uses the command line tools.
    // 写操作以及没有 libarchive 路径的格式所使用的后端，始终使用命令行工具。
    static ArchiveBackend *create(ArchiveFormat format);

    // Backend for reading an existing archive: libarchive when it is available
    // and the archive is not a split volume, otherwise the CLI backend.
    // 读取已有压缩包的后端：可用且非分卷时使用 libarchive，否则使用 CLI 后端。
    static ArchiveBackend *createForRead(ArchiveFormat format,
                                         const QString &path = QString());

    // Whether this build links libarchive (compile-time capability).
    // 当前构建是否链接 libarchive（编译期能力）。
    static bool isLibArchiveAvailable();

    // Troubleshooting escape hatch: QARCHIVER_NO_LIBARCHIVE forces the CLI.
    // 故障排查开关：QARCHIVER_NO_LIBARCHIVE 强制使用 CLI。
    static bool isLibArchiveDisabledByEnvironment();

    // Whether createForRead() would pick the in-process backend for this
    // archive (preference, environment, format whitelist, split volumes).
    // createForRead() 是否会为该压缩包选择进程内后端
    // （取决于偏好、环境变量、格式白名单与分卷情况）。
    static bool usesLibArchiveForRead(ArchiveFormat format, const QString &path);
};

#endif // QARCHIVER_CORE_ARCHIVEBACKENDFACTORY_H
