#include "EncodingDetector.h"

#include <QProcess>
#include <QStringConverter>

QString EncodingDetector::detect(const QByteArray &data)
{
    // Try UTF-8 first
    if (data.isValidUtf8()) {
        return QStringLiteral("UTF-8");
    }
    // Heuristic: GBK if bytes in the 0x80-0xFE range with typical patterns
    // This is a simplified heuristic; full detection would need ICU
    bool hasHighBytes = false;
    for (unsigned char c : data) {
        if (c >= 0x80) {
            hasHighBytes = true;
            break;
        }
    }
    if (hasHighBytes) {
        return QStringLiteral("GBK");
    }
    return QStringLiteral("UTF-8");
}

QString EncodingDetector::decode(const QByteArray &data, const QString &encodingHint)
{
    QString encoding = encodingHint;
    if (encoding.isEmpty()) {
        encoding = detect(data);
    }

    if (encoding.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return QString::fromUtf8(data);
    }

    // Qt6: use QStringConverter
    auto decoder = QStringDecoder(encoding.toUtf8().constData());
    if (decoder.isValid()) {
        return decoder(data);
    }

    // Fallback: interpret as local 8-bit
    return QString::fromLocal8Bit(data);
}

bool EncodingDetector::isLocaleAvailable(const QString &localeName)
{
    QProcess proc;
    proc.start(QStringLiteral("locale"), {QStringLiteral("-a")});
    if (!proc.waitForFinished(3000)) {
        return false;
    }
    const QByteArray output = proc.readAllStandardOutput();
    const QStringList locales = QString::fromUtf8(output).split('\n');
    for (const QString &l : locales) {
        if (l.trimmed().compare(localeName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
