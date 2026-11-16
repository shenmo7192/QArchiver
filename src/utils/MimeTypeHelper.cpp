#include "MimeTypeHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>
#include <array>
#include <cstdint>

#include "Logger.h"
#include "ProcessRunner.h"

// Magic Number signatures (design doc 4.3.2).  Compile-time byte arrays
// avoid runtime QByteArray::fromHex allocations.
namespace {

constexpr std::array<std::uint8_t, 6> kSig7z      = {0x37, 0x7a, 0xbc, 0xaf, 0x27, 0x1c};
constexpr std::array<std::uint8_t, 8> kSigRar5    = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x01, 0x00};
constexpr std::array<std::uint8_t, 7> kSigRar4    = {0x52, 0x61, 0x72, 0x21, 0x1a, 0x07, 0x00};
constexpr std::array<std::uint8_t, 4> kSigZip     = {0x50, 0x4b, 0x03, 0x04}; // local file header
constexpr std::array<std::uint8_t, 4> kSigZipEmpty = {0x50, 0x4b, 0x05, 0x06}; // empty archive
constexpr std::array<std::uint8_t, 4> kSigZipSpan = {0x50, 0x4b, 0x07, 0x08}; // spanned archive
constexpr std::array<std::uint8_t, 2> kSigGzip    = {0x1f, 0x8b};
constexpr std::array<std::uint8_t, 3> kSigBzip2   = {0x42, 0x5a, 0x68};
constexpr std::array<std::uint8_t, 6> kSigXz      = {0xfd, 0x37, 0x7a, 0x58, 0x5a, 0x00};
constexpr std::array<std::uint8_t, 5> kSigTar     = {0x75, 0x73, 0x74, 0x61, 0x72}; // "ustar" at 257

// List files in @p dir whose name matches @p regex, preserving the on-disk
// spelling (never lower-cases paths).
QStringList matchingFiles(const QDir &dir, const QRegularExpression &regex)
{
    QStringList result;
    const QStringList entries = dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (regex.match(entry).hasMatch()) {
            result << dir.absoluteFilePath(entry);
        }
    }
    return result;
}

// Stage 1.5: probe whether a compressed stream contains a tar (design doc 4.3.3)
ArchiveFormat resolveStreamFormat(const QString &filePath,
                                  ArchiveFormat tarFormat,
                                  ArchiveFormat streamFormat)
{
    const QString ext = MimeTypeHelper::extensionOf(filePath);

    const bool isTarSuffix = (ext == QStringLiteral(".tar.gz") || ext == QStringLiteral(".tgz")
                              || ext == QStringLiteral(".tar.xz") || ext == QStringLiteral(".txz")
                              || ext == QStringLiteral(".tar.bz2") || ext == QStringLiteral(".tbz2"));
    if (isTarSuffix) {
        return tarFormat;
    }

    QString decompressFlag;
    switch (tarFormat) {
    case ArchiveFormat::TarGz:  decompressFlag = QStringLiteral("-z"); break;
    case ArchiveFormat::TarBz2: decompressFlag = QStringLiteral("-j"); break;
    case ArchiveFormat::TarXz:  decompressFlag = QStringLiteral("-J"); break;
    default: break;
    }

    const QString tarPath = ProcessRunner::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        LOG_WARN("MimeTypeHelper", "tar tool not found; defaulting to stream format");
        return streamFormat;
    }

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    env.insert(QStringLiteral("LANG"), QStringLiteral("C"));

    QStringList args;
    args << QStringLiteral("-t");
    if (!decompressFlag.isEmpty()) {
        args << decompressFlag;
    }
    args << QStringLiteral("-f") << filePath;

    const ProcessResult result = ProcessRunner::run(
        tarPath, args, QString(), env, QString(), 3000);

    return result.success() ? tarFormat : streamFormat;
}

} // namespace

QByteArray MimeTypeHelper::readMagic(const QString &filePath, int maxBytes)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.read(maxBytes);
}

bool MimeTypeHelper::matchMagic(const QByteArray &head, std::span<const std::uint8_t> signature, int offset)
{
    if (offset < 0 || head.size() < offset + static_cast<int>(signature.size())) {
        return false;
    }
    const auto *data = reinterpret_cast<const std::uint8_t *>(head.constData());
    return std::equal(signature.begin(), signature.end(), data + offset);
}

QString MimeTypeHelper::extensionOf(const QString &filePath)
{
    const QString lower = filePath.toLower();

    static const QStringList compoundSuffixes = {
        QStringLiteral(".tar.gz"), QStringLiteral(".tar.xz"), QStringLiteral(".tar.bz2"),
        QStringLiteral(".tgz"), QStringLiteral(".txz"), QStringLiteral(".tbz2"),
        QStringLiteral(".7z.001"), QStringLiteral(".zip.001")
    };
    for (const QString &suffix : compoundSuffixes) {
        if (lower.endsWith(suffix)) {
            return suffix;
        }
    }

    if (lower.contains(QRegularExpression(QStringLiteral("\\.part\\d+\\.rar$")))) {
        return QStringLiteral(".partNN.rar");
    }
    if (lower.contains(QRegularExpression(QStringLiteral("\\.r\\d{2,}$")))) {
        return lower.mid(lower.lastIndexOf(QLatin1Char('.')));
    }

    const int dot = lower.lastIndexOf(QLatin1Char('.'));
    if (dot < 0) {
        return QString();
    }
    return lower.mid(dot);
}

bool MimeTypeHelper::isSplitVolume(const QString &filePath)
{
    static const QRegularExpression zipVol(
        QStringLiteral("\\.(?:7z|zip)\\.\\d{3,}$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rar4Vol(
        QStringLiteral("\\.r\\d{2,}$"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rar5Vol(
        QStringLiteral("\\.part\\d+\\.rar$"),
        QRegularExpression::CaseInsensitiveOption);

    return zipVol.match(filePath).hasMatch()
        || rar4Vol.match(filePath).hasMatch()
        || rar5Vol.match(filePath).hasMatch();
}

QString MimeTypeHelper::resolveFirstVolume(const QString &filePath)
{
    const QFileInfo info(filePath);
    const QDir dir = info.dir();

    // 7z/zip split: archive.7z.001 (preserve the original case).
    static const QRegularExpression zipVol(
        QStringLiteral("^(.*\\.(?:7z|zip))\\.\\d{3,}$"),
        QRegularExpression::CaseInsensitiveOption);
    auto match = zipVol.match(filePath);
    if (match.hasMatch()) {
        const QString base = QFileInfo(match.captured(1)).fileName();
        const QRegularExpression entryRe(
            QStringLiteral("^%1\\.\\d{3,}$").arg(QRegularExpression::escape(base)),
            QRegularExpression::CaseInsensitiveOption);
        QStringList volumes = matchingFiles(dir, entryRe);
        std::sort(volumes.begin(), volumes.end(), [](const QString &a, const QString &b) {
            static const QRegularExpression numberRe(QStringLiteral("\\.(\\d{3,})$"));
            const auto ma = numberRe.match(a);
            const auto mb = numberRe.match(b);
            if (ma.hasMatch() && mb.hasMatch()) {
                return ma.captured(1).toLongLong() < mb.captured(1).toLongLong();
            }
            return a < b;
        });
        if (!volumes.isEmpty()) {
            return volumes.first();
        }
        return filePath;
    }

    // RAR5 split: archive.partNN.rar
    static const QRegularExpression rar5Vol(
        QStringLiteral("^(.*)\\.part\\d+\\.rar$"),
        QRegularExpression::CaseInsensitiveOption);
    match = rar5Vol.match(filePath);
    if (match.hasMatch()) {
        const QString base = QFileInfo(match.captured(1)).fileName();
        const QRegularExpression entryRe(
            QStringLiteral("^%1\\.part(\\d+)\\.rar$").arg(QRegularExpression::escape(base)),
            QRegularExpression::CaseInsensitiveOption);
        QStringList volumes = matchingFiles(dir, entryRe);
        std::sort(volumes.begin(), volumes.end(), [](const QString &a, const QString &b) {
            static const QRegularExpression numberRe(QStringLiteral("\\.part(\\d+)\\.rar$"));
            const auto ma = numberRe.match(a);
            const auto mb = numberRe.match(b);
            if (ma.hasMatch() && mb.hasMatch()) {
                return ma.captured(1).toInt() < mb.captured(1).toInt();
            }
            return a < b;
        });
        if (!volumes.isEmpty()) {
            return volumes.first();
        }
        return filePath;
    }

    // RAR4 split: archive.rar + archive.r00...
    static const QRegularExpression rar4Vol(
        QStringLiteral("^(.*)\\.r\\d{2,}$"),
        QRegularExpression::CaseInsensitiveOption);
    match = rar4Vol.match(filePath);
    if (match.hasMatch()) {
        const QString base = QFileInfo(match.captured(1)).fileName();
        const QRegularExpression entryRe(
            QStringLiteral("^%1\\.rar$").arg(QRegularExpression::escape(base)),
            QRegularExpression::CaseInsensitiveOption);
        const QStringList matches = matchingFiles(dir, entryRe);
        if (!matches.isEmpty()) {
            return matches.first();
        }
        return filePath;
    }

    return filePath;
}

ArchiveFormat MimeTypeHelper::detect(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isReadable()) {
        LOG_WARN("MimeTypeHelper", "File not readable: %s", filePath.toUtf8().constData());
        return ArchiveFormat::Unknown;
    }

    const QByteArray head = readMagic(filePath);
    if (head.isEmpty()) {
        return ArchiveFormat::Unknown;
    }

    // Stage 1: Magic Number
    if (matchMagic(head, kSigRar5, 0)) return ArchiveFormat::Rar5;
    if (matchMagic(head, kSigRar4, 0)) return ArchiveFormat::Rar4;
    if (matchMagic(head, kSig7z, 0))   return ArchiveFormat::SevenZip;
    if (matchMagic(head, kSigZip, 0)
        || matchMagic(head, kSigZipEmpty, 0)
        || matchMagic(head, kSigZipSpan, 0)) {
        return ArchiveFormat::Zip;
    }

    if (matchMagic(head, kSigGzip, 0)) {
        return resolveStreamFormat(filePath, ArchiveFormat::TarGz, ArchiveFormat::Gzip);
    }
    if (matchMagic(head, kSigBzip2, 0)) {
        return resolveStreamFormat(filePath, ArchiveFormat::TarBz2, ArchiveFormat::Bzip2);
    }
    if (matchMagic(head, kSigXz, 0)) {
        return resolveStreamFormat(filePath, ArchiveFormat::TarXz, ArchiveFormat::Xz);
    }
    if (matchMagic(head, kSigTar, 257)) {
        return ArchiveFormat::Tar;
    }

    // Stage 2: suffix fallback
    const QString ext = extensionOf(filePath);
    LOG_DEBUG("MimeTypeHelper", "No magic match; falling back to suffix: %s",
              ext.toUtf8().constData());

    if (ext == QStringLiteral(".tar")) return ArchiveFormat::Tar;
    if (ext == QStringLiteral(".tar.gz") || ext == QStringLiteral(".tgz"))
        return ArchiveFormat::TarGz;
    if (ext == QStringLiteral(".tar.xz") || ext == QStringLiteral(".txz"))
        return ArchiveFormat::TarXz;
    if (ext == QStringLiteral(".tar.bz2") || ext == QStringLiteral(".tbz2"))
        return ArchiveFormat::TarBz2;
    if (ext == QStringLiteral(".zip") || ext == QStringLiteral(".zip.001"))
        return ArchiveFormat::Zip;
    if (ext == QStringLiteral(".7z") || ext == QStringLiteral(".7z.001"))
        return ArchiveFormat::SevenZip;
    if (ext == QStringLiteral(".rar") || ext == QStringLiteral(".partNN.rar"))
        return ArchiveFormat::Rar5;
    if (ext.startsWith(QStringLiteral(".r")) && ext.size() >= 4)
        return ArchiveFormat::Rar4;
    if (ext == QStringLiteral(".gz")) return ArchiveFormat::Gzip;
    if (ext == QStringLiteral(".bz2")) return ArchiveFormat::Bzip2;
    if (ext == QStringLiteral(".xz")) return ArchiveFormat::Xz;

    LOG_WARN("MimeTypeHelper", "Unrecognized format: %s", filePath.toUtf8().constData());
    return ArchiveFormat::Unknown;
}
