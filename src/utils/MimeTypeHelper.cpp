#include "MimeTypeHelper.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QRegularExpression>
#include <QProcess>
#include <algorithm>

#include "Logger.h"
#include "ProcessHelper.h"

// Magic Number signatures (design doc 4.3.2)
static const QByteArray kSig7z     = QByteArray::fromHex("377abcaf271c");                 // 6 bytes
static const QByteArray kSigRar5  = QByteArray::fromHex("526172211a070100");           // 8 bytes
static const QByteArray kSigRar4  = QByteArray::fromHex("526172211a0700");             // 7 bytes
static const QByteArray kSigZip   = QByteArray::fromHex("504b0304");                     // 4 bytes
static const QByteArray kSigGzip  = QByteArray::fromHex("1f8b");                         // 2 bytes
static const QByteArray kSigBzip2 = QByteArray::fromHex("425a68");                       // 3 bytes
static const QByteArray kSigXz    = QByteArray::fromHex("fd377a585a00");                 // 6 bytes
static const QByteArray kSigTar   = QByteArray::fromHex("7573746172");                   // "ustar" at offset 257

// Stage 1.5: probe whether a compressed stream contains a tar (design doc 4.3.3)
// Runs `tar -t -f <file>` with the appropriate decompression flag; exit code 0 => tar
static ArchiveFormat resolveStreamFormat(const QString &filePath, ArchiveFormat tarFormat, ArchiveFormat streamFormat)
{
    const QString ext = MimeTypeHelper::extensionOf(filePath);

    // If suffix clearly indicates tar variant and matches the outer signature, skip probe
    const bool isTarSuffix = (ext == QStringLiteral(".tar.gz") || ext == QStringLiteral(".tgz")
                              || ext == QStringLiteral(".tar.xz") || ext == QStringLiteral(".txz")
                              || ext == QStringLiteral(".tar.bz2") || ext == QStringLiteral(".tbz2"));
    if (isTarSuffix) {
        return tarFormat;
    }

    // Determine decompression flag based on outer signature
    QString decompressFlag;
    switch (tarFormat) {
    case ArchiveFormat::TarGz:  decompressFlag = QStringLiteral("-z"); break;
    case ArchiveFormat::TarBz2: decompressFlag = QStringLiteral("-j"); break;
    case ArchiveFormat::TarXz:  decompressFlag = QStringLiteral("-J"); break;
    default: break;
    }

    const QString tarPath = ProcessHelper::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        LOG_WARN("MimeTypeHelper", "tar tool not found; defaulting to stream format");
        return streamFormat;
    }

    // Probe with 3-second timeout
    QProcess proc;
    QStringList args = {QStringLiteral("-t"), decompressFlag, QStringLiteral("-f"), filePath};
    proc.setStandardOutputFile(QProcess::nullDevice());
    proc.setStandardErrorFile(QProcess::nullDevice());
    proc.start(tarPath, args);
    if (!proc.waitForFinished(3000)) {
        proc.kill();
        proc.waitForFinished(1000);
        // Timeout: fallback to suffix; if suffix is pure stream, treat as stream
        if (ext == QStringLiteral(".gz") || ext == QStringLiteral(".bz2") || ext == QStringLiteral(".xz")) {
            return streamFormat;
        }
        return streamFormat;
    }

    if (proc.exitCode() == 0) {
        return tarFormat;
    }
    return streamFormat;
}

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
    if (head.size() < offset + signature.size()) {
        return false;
    }
    return (head.mid(offset, signature.size()) == signature);
}

QString MimeTypeHelper::extensionOf(const QString &filePath)
{
    const QString lower = filePath.toLower();
    // Compound suffixes (longest first)
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
    // RAR5 split: .partNN.rar
    if (lower.contains(QRegularExpression(QStringLiteral("\\.part\\d+\\.rar$")))) {
        return QStringLiteral(".partNN.rar");
    }
    // RAR4 split: .rNN
    if (lower.contains(QRegularExpression(QStringLiteral("\\.r\\d{2,}$")))) {
        return lower.mid(lower.lastIndexOf('.'));
    }
    int dot = lower.lastIndexOf('.');
    if (dot < 0) {
        return QString();
    }
    return lower.mid(dot);
}

bool MimeTypeHelper::isSplitVolume(const QString &filePath)
{
    const QString lower = filePath.toLower();
    if (lower.contains(QRegularExpression(QStringLiteral("\\.(?:7z|zip)\\.\\d{3,}$")))) {
        return true;
    }
    if (lower.contains(QRegularExpression(QStringLiteral("\\.r\\d{2,}$")))) {
        return true;
    }
    if (lower.contains(QRegularExpression(QStringLiteral("\\.part\\d+\\.rar$")))) {
        return true;
    }
    return false;
}

QString MimeTypeHelper::resolveFirstVolume(const QString &filePath)
{
    const QString lower = filePath.toLower();
    QFileInfo fi(filePath);
    QDir dir = fi.dir();

    // 7z/zip split: archive.7z.001
    static const QRegularExpression zip7zVol(QStringLiteral("(.*\\.(?:7z|zip))\\.\\d{3,}$"));
    auto m = zip7zVol.match(lower);
    if (m.hasMatch()) {
        QString base = m.captured(1);
        for (int i = 1; i <= 999; ++i) {
            QString vol = QStringLiteral("%1.%2").arg(base).arg(i, 3, 10, QChar('0'));
            QFileInfo volFi(dir, vol);
            if (volFi.exists()) {
                return volFi.absoluteFilePath();
            }
        }
        return filePath;
    }

    // RAR5 split: archive.partNN.rar
    static const QRegularExpression rar5Vol(QStringLiteral("(.*)\\.part\\d+\\.rar$"));
    m = rar5Vol.match(lower);
    if (m.hasMatch()) {
        QString base = m.captured(1);
        QStringList volumes;
        for (int i = 1; i <= 999; ++i) {
            for (int w = 1; w <= 3; ++w) {
                QString vol = QStringLiteral("%1.part%2.rar").arg(base).arg(i, w, 10, QChar('0'));
                QFileInfo volFi(dir, vol);
                if (volFi.exists()) {
                    volumes << volFi.absoluteFilePath();
                }
            }
        }
        if (!volumes.isEmpty()) {
            std::sort(volumes.begin(), volumes.end(), [](const QString &a, const QString &b) {
                static const QRegularExpression re(QStringLiteral("part(\\d+)"));
                auto ma = re.match(a);
                auto mb = re.match(b);
                if (ma.hasMatch() && mb.hasMatch()) {
                    return ma.captured(1).toInt() < mb.captured(1).toInt();
                }
                return a < b;
            });
            return volumes.first();
        }
        return filePath;
    }

    // RAR4 split: archive.rar + archive.r00...
    static const QRegularExpression rar4Vol(QStringLiteral("(.*)\\.r\\d{2,}$"));
    m = rar4Vol.match(lower);
    if (m.hasMatch()) {
        QString base = m.captured(1);
        QString mainRar = QFileInfo(dir, base + QStringLiteral(".rar")).absoluteFilePath();
        if (QFileInfo::exists(mainRar)) {
            return mainRar;
        }
        return filePath;
    }

    return filePath;
}

ArchiveFormat MimeTypeHelper::detect(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable()) {
        LOG_WARN("MimeTypeHelper", "File not readable: %s", filePath.toUtf8().constData());
        return ArchiveFormat::Unknown;
    }

    const QByteArray head = readMagic(filePath);
    if (head.isEmpty()) {
        return ArchiveFormat::Unknown;
    }

    // Stage 1: Magic Number (order: RAR5 -> RAR4 -> 7z -> ZIP -> gzip -> bzip2 -> xz -> tar)
    if (matchMagic(head, kSigRar5, 0)) return ArchiveFormat::Rar5;
    if (matchMagic(head, kSigRar4, 0)) return ArchiveFormat::Rar4;
    if (matchMagic(head, kSig7z, 0))   return ArchiveFormat::SevenZip;
    if (matchMagic(head, kSigZip, 0))  return ArchiveFormat::Zip;

    // gzip stream (could be tar.gz or pure .gz)
    if (matchMagic(head, kSigGzip, 0)) {
        return resolveStreamFormat(filePath, ArchiveFormat::TarGz, ArchiveFormat::Gzip);
    }
    // bzip2 stream
    if (matchMagic(head, kSigBzip2, 0)) {
        return resolveStreamFormat(filePath, ArchiveFormat::TarBz2, ArchiveFormat::Bzip2);
    }
    // xz stream
    if (matchMagic(head, kSigXz, 0)) {
        return resolveStreamFormat(filePath, ArchiveFormat::TarXz, ArchiveFormat::Xz);
    }
    // plain tar (ustar at offset 257)
    if (matchMagic(head, kSigTar, 257)) return ArchiveFormat::Tar;

    // Stage 2: suffix fallback
    const QString ext = extensionOf(filePath);
    LOG_DEBUG("MimeTypeHelper", "No magic match; falling back to suffix: %s", ext.toUtf8().constData());

    if (ext == QStringLiteral(".tar")) return ArchiveFormat::Tar;
    if (ext == QStringLiteral(".tar.gz") || ext == QStringLiteral(".tgz"))
        return ArchiveFormat::TarGz;
    if (ext == QStringLiteral(".tar.xz") || ext == QStringLiteral(".txz"))
        return ArchiveFormat::TarXz;
    if (ext == QStringLiteral(".tar.bz2") || ext == QStringLiteral(".tbz2"))
        return ArchiveFormat::TarBz2;
    if (ext == QStringLiteral(".zip")) return ArchiveFormat::Zip;
    if (ext == QStringLiteral(".7z")) return ArchiveFormat::SevenZip;
    if (ext == QStringLiteral(".rar")) return ArchiveFormat::Rar5;
    if (ext == QStringLiteral(".gz")) return ArchiveFormat::Gzip;
    if (ext == QStringLiteral(".bz2")) return ArchiveFormat::Bzip2;
    if (ext == QStringLiteral(".xz")) return ArchiveFormat::Xz;

    // Stage 3: Unknown
    LOG_WARN("MimeTypeHelper", "Unrecognized format: %s", filePath.toUtf8().constData());
    return ArchiveFormat::Unknown;
}
