#ifndef ARCHIVECONSTANTS_H
#define ARCHIVECONSTANTS_H

// QArchiver / 小Q解压
// Archive format, overwrite policy enums and shared format helpers.
// 压缩格式、覆盖策略枚举以及共用的格式辅助函数。

#include <QString>
#include <QMetaType>

// Archive format enumeration / 压缩格式枚举
enum class ArchiveFormat {
    Unknown,    // Unrecognized / corrupted
    Tar,        // Plain .tar
    TarGz,      // .tar.gz / .tgz
    TarXz,      // .tar.xz / .txz
    TarBz2,     // .tar.bz2 / .tbz2
    Zip,        // .zip (handled via 7z)
    SevenZip,   // .7z
    Rar4,       // RAR4
    Rar5,       // RAR5
    Gzip,       // Pure .gz stream
    Bzip2,      // Pure .bz2 stream
    Xz          // Pure .xz stream
};

Q_DECLARE_METATYPE(ArchiveFormat)

// Overwrite policy for extraction / 解压覆盖策略
enum class OverwritePolicy {
    Ask,        // Ask the user for each conflict
    Always,     // Always overwrite
    Never,      // Never overwrite (skip)
    Rename      // Auto-rename conflicting files
};

Q_DECLARE_METATYPE(OverwritePolicy)

// Convert ArchiveFormat to a human-readable string / 转换为可读字符串
inline QString formatToString(ArchiveFormat fmt)
{
    switch (fmt) {
    case ArchiveFormat::Tar:     return QStringLiteral("tar");
    case ArchiveFormat::TarGz:   return QStringLiteral("tar.gz");
    case ArchiveFormat::TarXz:   return QStringLiteral("tar.xz");
    case ArchiveFormat::TarBz2:  return QStringLiteral("tar.bz2");
    case ArchiveFormat::Zip:     return QStringLiteral("zip");
    case ArchiveFormat::SevenZip:return QStringLiteral("7z");
    case ArchiveFormat::Rar4:    return QStringLiteral("rar (v4)");
    case ArchiveFormat::Rar5:    return QStringLiteral("rar (v5)");
    case ArchiveFormat::Gzip:    return QStringLiteral("gzip");
    case ArchiveFormat::Bzip2:   return QStringLiteral("bzip2");
    case ArchiveFormat::Xz:      return QStringLiteral("xz");
    case ArchiveFormat::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

// Check whether a format is a tar variant / 判断是否为 tar 系列格式
inline bool isTarFamily(ArchiveFormat fmt)
{
    return fmt == ArchiveFormat::Tar
        || fmt == ArchiveFormat::TarGz
        || fmt == ArchiveFormat::TarXz
        || fmt == ArchiveFormat::TarBz2;
}

// Check whether a format is a pure compression stream / 判断是否为纯压缩流
inline bool isStreamFamily(ArchiveFormat fmt)
{
    return fmt == ArchiveFormat::Gzip
        || fmt == ArchiveFormat::Bzip2
        || fmt == ArchiveFormat::Xz;
}

// Check whether a format is a RAR variant / 判断是否为 RAR 系列格式
inline bool isRarFamily(ArchiveFormat fmt)
{
    return fmt == ArchiveFormat::Rar4 || fmt == ArchiveFormat::Rar5;
}

// Default extension for a given format / 返回格式的默认扩展名
inline QString defaultExtension(ArchiveFormat fmt)
{
    switch (fmt) {
    case ArchiveFormat::Tar:      return QStringLiteral(".tar");
    case ArchiveFormat::TarGz:    return QStringLiteral(".tar.gz");
    case ArchiveFormat::TarXz:    return QStringLiteral(".tar.xz");
    case ArchiveFormat::TarBz2:   return QStringLiteral(".tar.bz2");
    case ArchiveFormat::Zip:      return QStringLiteral(".zip");
    case ArchiveFormat::SevenZip: return QStringLiteral(".7z");
    case ArchiveFormat::Rar4:
    case ArchiveFormat::Rar5:     return QStringLiteral(".rar");
    case ArchiveFormat::Gzip:     return QStringLiteral(".gz");
    case ArchiveFormat::Bzip2:    return QStringLiteral(".bz2");
    case ArchiveFormat::Xz:       return QStringLiteral(".xz");
    case ArchiveFormat::Unknown:  return QString();
    }
    return QString();
}

#endif // ARCHIVECONSTANTS_H
