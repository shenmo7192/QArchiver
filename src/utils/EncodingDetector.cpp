#include "EncodingDetector.h"

#include <QProcess>
#include <QStringConverter>

#include <cerrno>
#include <iconv.h>

namespace {

// RAII wrapper for the libc iconv handle so every early return closes it.
class IconvHandle {
public:
    explicit IconvHandle(const char *to, const char *from)
        : m_cd(iconv_open(to, from))
    {
    }

    ~IconvHandle()
    {
        if (valid()) {
            iconv_close(m_cd);
        }
    }

    IconvHandle(const IconvHandle &) = delete;
    IconvHandle &operator=(const IconvHandle &) = delete;

    bool valid() const { return m_cd != reinterpret_cast<iconv_t>(-1); }
    iconv_t get() const { return m_cd; }

private:
    iconv_t m_cd;
};

// Qt 6 only ships UTF-8/UTF-16/UTF-32/Latin-1 converters unless it was built
// with ICU.  GBK/Big5/Shift-JIS are therefore handled through the libc iconv
// implementation, which is available on all supported Linux platforms.
QString fromIconv(const QByteArray &data, const char *fromEncoding)
{
    if (data.isEmpty()) {
        return QString();
    }

    IconvHandle cd("UTF-8", fromEncoding);
    if (!cd.valid()) {
        return QString();
    }

    QByteArray output;
    output.resize(data.size() * 4 + 16);

    char *input = const_cast<char *>(data.constData());
    size_t inputLeft = static_cast<size_t>(data.size());
    char *out = output.data();
    size_t outLeft = static_cast<size_t>(output.size());

    while (inputLeft > 0) {
        const size_t rc = iconv(cd.get(), &input, &inputLeft, &out, &outLeft);
        if (rc == static_cast<size_t>(-1)) {
            if (errno == E2BIG) {
                const int used = static_cast<int>(output.size() - outLeft);
                output.resize(output.size() * 2);
                out = output.data() + used;
                outLeft = static_cast<size_t>(output.size() - used);
                continue;
            }
            return QString();
        }
    }

    const int used = static_cast<int>(output.size() - outLeft);
    return QString::fromUtf8(output.constData(), used);
}

QString decodeWithIconv(const QByteArray &data, const QString &encodingHint)
{
    if (encodingHint.compare(QStringLiteral("GBK"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("GB2312"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("GB18030"), Qt::CaseInsensitive) == 0) {
        return fromIconv(data, "GB18030");
    }
    if (encodingHint.compare(QStringLiteral("Big5"), Qt::CaseInsensitive) == 0) {
        return fromIconv(data, "BIG5");
    }
    if (encodingHint.compare(QStringLiteral("Shift-JIS"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("SJIS"), Qt::CaseInsensitive) == 0) {
        return fromIconv(data, "SHIFT-JIS");
    }
    if (encodingHint.compare(QStringLiteral("ISO-8859-1"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("Latin1"), Qt::CaseInsensitive) == 0) {
        return QString::fromLatin1(data);
    }

    // Try Qt's built-in converters first (UTF-8/UTF-16/...).
    const auto encoding = QStringConverter::encodingForName(encodingHint.toUtf8().constData());
    if (encoding.has_value()) {
        QStringDecoder decoder(*encoding);
        if (decoder.isValid()) {
            const QString decoded = decoder(data);
            if (!decoder.hasError()) {
                return decoded;
            }
        }
    }

    return fromIconv(data, encodingHint.toUtf8().constData());
}

} // namespace

QString EncodingDetector::detect(const QByteArray &data)
{
    if (data.isEmpty()) {
        return QStringLiteral("UTF-8");
    }
    if (data.isValidUtf8()) {
        return QStringLiteral("UTF-8");
    }

    // GB18030 is a superset of GBK and accepts the widest range of Chinese
    // byte sequences.  Try it before the other legacy encodings.
    if (!fromIconv(data, "GB18030").isEmpty()) {
        return QStringLiteral("GB18030");
    }
    if (!fromIconv(data, "BIG5").isEmpty()) {
        return QStringLiteral("Big5");
    }
    if (!fromIconv(data, "SHIFT-JIS").isEmpty()) {
        return QStringLiteral("Shift-JIS");
    }

    return QStringLiteral("ISO-8859-1");
}

QString EncodingDetector::decode(const QByteArray &data, const QString &encodingHint)
{
    const QString encoding = encodingHint.isEmpty() ? detect(data) : encodingHint;

    if (encoding.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return QString::fromUtf8(data);
    }

    const QString decoded = decodeWithIconv(data, encoding);
    if (!decoded.isNull()) {
        return decoded;
    }

    // Last resort: interpret as the local 8-bit encoding.
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
    const QStringList locales = QString::fromUtf8(output).split(QLatin1Char('\n'));
    for (const QString &locale : locales) {
        if (locale.trimmed().compare(localeName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}
