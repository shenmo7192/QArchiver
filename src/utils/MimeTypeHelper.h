#ifndef MIMETYPEHELPER_H
#define MIMETYPEHELPER_H

// QArchiver / 小Q解压
// Archive format detection based on magic bytes, stream probes and suffixes.
// 基于魔数、流探测与扩展名的压缩格式检测。

#include <QString>
#include <QByteArray>

#include <cstdint>
#include <span>

#include "common/ArchiveConstants.h"

// Format detection authority (design doc 4.3).
// Three-stage detection: Magic Number -> tar stream probe -> suffix fallback.
// 格式检测入口（设计文档 4.3）。
// 三阶段检测：魔数 -> tar 流探测 -> 扩展名兜底。
class MimeTypeHelper {
public:
    // Main entry: returns detected format; Unknown if unrecognizable
    // 主入口：返回检测到的格式；无法识别时返回 Unknown
    static ArchiveFormat detect(const QString &filePath);

    // Read file head bytes (default 512 bytes, covers all target formats)
    // 读取文件头字节（默认 512 字节，足以覆盖所有目标格式）
    static QByteArray readMagic(const QString &filePath, int maxBytes = 512);

    // Match a signature at a given offset / 在指定偏移处匹配文件签名
    static bool matchMagic(const QByteArray &head, std::span<const std::uint8_t> signature, int offset);

    // Extract lowercase extension (normalizes .tgz/.txz/.tbz2)
    // 提取小写扩展名（规范化 .tgz/.txz/.tbz2）
    static QString extensionOf(const QString &filePath);

    // Check if the archive is a split volume (e.g. .7z.001, .zip.001)
    // 判断是否为分卷压缩包（例如 .7z.001、.zip.001）
    static bool isSplitVolume(const QString &filePath);

    // Resolve the first volume path from any volume file
    // 从任意分卷文件解析出首卷路径
    static QString resolveFirstVolume(const QString &filePath);
};

#endif // MIMETYPEHELPER_H
