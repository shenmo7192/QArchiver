#include "MimeTypeHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <algorithm>

#include "Logger.h"
#include "ProcessRunner.h"

// Magic Number signatures (design doc 4.3.2)
static const QByteArray kSig7z        = QByteArray::fromHex("377abcaf271c");       // 6 bytes
static const QByteArray kSigRar5     = QByteArray::fromHex("526172211a070100");   // 8 bytes
static const QByteArray kSigRar4     = QByteArray::fromHex("526172211a0700");     // 7 bytes
static const QByteArray kSigZip      = QByteArray::fromHex("504b0304");           // local file header
static const QByteArray kSigZipEmpty = QByteArray::fromHex("504b0506");           // empty archive
static const QByteArray kSigZipSpan  = QByteArray::fromHex("504b0708");           // spanned archive
static const QByteArray kSigGzip     = QByteArray::fromHex("1f8b");
static const QByteArray kSigBzip2    = QByteArray::fromHex("425a68");
static const QByteArray kSigXz       = QByteArray::fromHex("fd377a585a00");
static const QByteArray kSigTar      = QByteArray::fromHex("7573746172");         // "ustar" at 257

namespace {

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

bool MimeTypeHelper::matchMagic(const QByteArray &head, const QByteArray &signature, int offset)
{
    if (offset < 0 || head.size() < offset + signature.size()) {
        return false;
    }
    return head.mid(offset, signature.size()) == signature;
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
