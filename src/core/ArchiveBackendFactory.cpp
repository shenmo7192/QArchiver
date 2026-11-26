// QArchiver / 小Q解压
// Backend factory: chooses between the CLI backends and the in-process
// libarchive backend.
// 后端工厂：在命令行后端与进程内 libarchive 后端之间选择。

#include "core/ArchiveBackendFactory.h"

#include "backends/ArchiveBackend.h"
#include "backends/iso/IsoBackend.h"
#include "backends/libarchive/LibArchiveBackend.h"
#include "backends/rar/RarBackend.h"
#include "backends/sevenzip/SevenZipBackend.h"
#include "backends/sevenzip/ZipBackend.h"
#include "backends/stream/StreamBackend.h"
#include "backends/tar/TarBackend.h"

#include "common/FormatRegistry.h"
#include "core/AppSettings.h"
#include "utils/Logger.h"
#include "utils/MimeTypeHelper.h"

#ifdef QARCHIVER_HAS_LIBARCHIVE
namespace {

// Formats the libarchive backend can read. RAR keeps using the bundled 7-Zip
// helper through RarBackend, and volumes are always left to the CLI (libarchive
// has no volume support at all).  Disc images are read by libarchive when it
// supports ISO9660 (Rock Ridge/Joliet); UDF images fail here and are retried on
// the CLI backend by ArchiveManager.
// libarchive 后端可读取的格式。RAR 继续通过 RarBackend 使用随程序分发的
// 7-Zip 辅助程序，分卷始终交给命令行后端（libarchive 完全不支持分卷）。
// 光盘镜像在 libarchive 支持 ISO9660（Rock Ridge/Joliet）时由它读取；
// UDF 镜像在此失败，随后由 ArchiveManager 改用命令行后端重试。
bool supportsLibArchiveRead(ArchiveFormat fmt)
{
    switch (fmt) {
    case ArchiveFormat::Zip:
    case ArchiveFormat::SevenZip:
    case ArchiveFormat::Tar:
    case ArchiveFormat::TarGz:
    case ArchiveFormat::TarXz:
    case ArchiveFormat::TarBz2:
    case ArchiveFormat::Gzip:
    case ArchiveFormat::Bzip2:
    case ArchiveFormat::Xz:
    case ArchiveFormat::Iso:
        return true;
    case ArchiveFormat::TarZst:
    case ArchiveFormat::Zstd:
        // Keep zstd on the bundled 7-Zip backend; libarchive builds are not
        // guaranteed to include the zstd filter.
        // zstd 继续使用内置 7-Zip；不能假定 libarchive 编译时包含 zstd 过滤器。
        return false;
    case ArchiveFormat::Rar4:
    case ArchiveFormat::Rar5:
    case ArchiveFormat::Unknown:
        return false;
    }
    return false;
}
} // namespace
#endif // QARCHIVER_HAS_LIBARCHIVE

ArchiveBackend *ArchiveBackendFactory::create(ArchiveFormat fmt)
{
    switch (fmt) {
    case ArchiveFormat::Tar:
    case ArchiveFormat::TarGz:
    case ArchiveFormat::TarXz:
    case ArchiveFormat::TarBz2:
    case ArchiveFormat::TarZst:
        return new TarBackend(fmt);
    case ArchiveFormat::Zip:
        return new ZipBackend();
    case ArchiveFormat::SevenZip:
        return new SevenZipBackend();
    case ArchiveFormat::Rar4:
        return new RarBackend(ArchiveFormat::Rar4);
    case ArchiveFormat::Rar5:
        return new RarBackend(ArchiveFormat::Rar5);
    case ArchiveFormat::Gzip:
    case ArchiveFormat::Bzip2:
    case ArchiveFormat::Xz:
    case ArchiveFormat::Zstd:
        return new StreamBackend(fmt);
    case ArchiveFormat::Iso:
        return new IsoBackend();
    case ArchiveFormat::Unknown:
        return nullptr;
    }
    return nullptr;
}

bool ArchiveBackendFactory::usesLibArchiveForRead(ArchiveFormat fmt, const QString &path)
{
#ifdef QARCHIVER_HAS_LIBARCHIVE
    return !isLibArchiveDisabledByEnvironment()
        && AppSettings::instance().useLibArchiveBackend()
        && !path.isEmpty() && supportsLibArchiveRead(fmt)
        && !FormatRegistry::isSplitVolume(path);
#else
    Q_UNUSED(fmt);
    Q_UNUSED(path);
    return false;
#endif
}

ArchiveBackend *ArchiveBackendFactory::createForRead(ArchiveFormat fmt,
                                                  const QString &path)
{
#ifdef QARCHIVER_HAS_LIBARCHIVE
    if (usesLibArchiveForRead(fmt, path)) {
        return new LibArchiveBackend(fmt);
    }
#else
    Q_UNUSED(path);
#endif
    return create(fmt);
}

bool ArchiveBackendFactory::isLibArchiveAvailable()
{
#ifdef QARCHIVER_HAS_LIBARCHIVE
    return true;
#else
    return false;
#endif
}

bool ArchiveBackendFactory::isLibArchiveDisabledByEnvironment()
{
    return qEnvironmentVariableIsSet("QARCHIVER_NO_LIBARCHIVE");
}
