// QArchiver / 小Q解压
// UTF-8/GB18030/Big5/Shift-JIS detection, decoding and code-page mapping.
// UTF-8/GB18030/Big5/Shift-JIS 检测、解码及代码页映射。

#include "utils/EncodingDetector.h"

#include <QProcess>
#include <QStringConverter>

#include <climits>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <iconv.h>
#endif

namespace {

#ifdef Q_OS_WIN

// Windows provides the same legacy code pages through MultiByteToWideChar, so
// iconv is not required (MSVC does not ship it).
// Windows 通过 MultiByteToWideChar 提供相同的旧代码页，因此不需要 iconv
// （MSVC 不提供 iconv）。
UINT codePageForName(const char *name)
{
    const QByteArray normalized = QByteArray(name).toUpper();
    if (normalized == "UTF-8" || normalized == "UTF8"
        || normalized == "CP65001") {
        return 65001;
    }
    if (normalized == "GBK" || normalized == "GB2312"
        || normalized == "GB18030" || normalized == "CP936") {
        return 54936; // GB18030; the retry below falls back to CP936.
    }
    if (normalized == "BIG5" || normalized == "CP950") {
        return 950;
    }
    if (normalized == "SHIFT-JIS" || normalized == "SJIS"
        || normalized == "CP932") {
        return 932;
    }
    if (normalized == "ISO-8859-1" || normalized == "LATIN1") {
        return 28591;
    }
    return 0;
}

QString fromIconv(const QByteArray &data, const char *fromEncoding)
{
    if (data.isEmpty() || data.size() > INT_MAX) {
        return QString();
    }
    UINT codePage = codePageForName(fromEncoding);
    if (codePage == 0) {
        return QString();
    }

    int length = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS,
                                     data.constData(),
                                     static_cast<int>(data.size()), nullptr, 0);
    if (length <= 0 && codePage == 54936) {
        // Some Windows versions reject MB_ERR_INVALID_CHARS for CP54936.
        codePage = 936;
        length = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS,
                                     data.constData(),
                                     static_cast<int>(data.size()), nullptr, 0);
    }
    if (length <= 0) {
        return QString();
    }

    QString result;
    result.resize(length);
    const int written = MultiByteToWideChar(
        codePage, MB_ERR_INVALID_CHARS, data.constData(),
        static_cast<int>(data.size()), reinterpret_cast<LPWSTR>(result.data()),
        length);
    if (written <= 0) {
        return QString();
    }
    result.resize(written);
    return result;
}

QByteArray toIconv(const QString &text, const char *toEncoding)
{
    if (text.isEmpty() || text.size() > INT_MAX) {
        return QByteArray();
    }
    const UINT codePage = codePageForName(toEncoding);
    if (codePage == 0 || codePage == 65001) {
        return text.toUtf8();
    }

    BOOL usedDefault = FALSE;
    const int length = WideCharToMultiByte(
        codePage, WC_NO_BEST_FIT_CHARS,
        reinterpret_cast<LPCWCH>(text.utf16()), static_cast<int>(text.size()),
        nullptr, 0, nullptr, &usedDefault);
    if (length <= 0 || usedDefault) {
        // The text contains characters that this code page cannot represent.
        return QByteArray();
    }

    QByteArray result;
    result.resize(length);
    const int written = WideCharToMultiByte(
        codePage, WC_NO_BEST_FIT_CHARS,
        reinterpret_cast<LPCWCH>(text.utf16()), static_cast<int>(text.size()),
        result.data(), length, nullptr, &usedDefault);
    if (written <= 0 || usedDefault) {
        return QByteArray();
    }
    result.resize(written);
    return result;
}

#else // !Q_OS_WIN

// RAII wrapper for the libc iconv handle so every early return closes it.
// libc iconv 句柄的 RAII 封装，确保每个提前返回路径都会关闭它。
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
// 除非启用 ICU，Qt 6 只内置 UTF-8/UTF-16/UTF-32/Latin-1 转换器。
// 因此 GBK/Big5/Shift-JIS 通过 libc iconv 处理，所有支持的 Linux 平台均可用。
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

QByteArray toIconv(const QString &text, const char *toEncoding)
{
    if (text.isEmpty()) {
        return QByteArray();
    }

    const QByteArray utf8 = text.toUtf8();
    IconvHandle cd(toEncoding, "UTF-8");
    if (!cd.valid()) {
        return QByteArray();
    }

    QByteArray output;
    output.resize(utf8.size() * 4 + 16);

    char *input = const_cast<char *>(utf8.constData());
    size_t inputLeft = static_cast<size_t>(utf8.size());
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
            return QByteArray();
        }
    }

    const int used = static_cast<int>(output.size() - outLeft);
    output.resize(used);
    return output;
}

#endif // Q_OS_WIN

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
    // 优先尝试 Qt 内置转换器（UTF-8/UTF-16 等）。
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
    // GB18030 是 GBK 的超集，可接受范围最广的中文字节序列。
    // 因此优先于其他旧编码尝试。
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
    // 最后兜底：按本地 8 位编码解释。
    return QString::fromLocal8Bit(data);
}

QByteArray EncodingDetector::encode(const QString &text, const QString &encodingHint)
{
    if (encodingHint.isEmpty()
        || encodingHint.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return text.toUtf8();
    }
    if (encodingHint.compare(QStringLiteral("GBK"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("GB2312"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("GB18030"), Qt::CaseInsensitive) == 0) {
        // 7-Zip's -scs936 expects CP936/GBK, not the four-byte GB18030 form.
        // 7-Zip 的 -scs936 需要 CP936/GBK，而不是四字节 GB18030 形式。
        return toIconv(text, "GBK");
    }
    if (encodingHint.compare(QStringLiteral("Big5"), Qt::CaseInsensitive) == 0) {
        return toIconv(text, "BIG5");
    }
    if (encodingHint.compare(QStringLiteral("Shift-JIS"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("SJIS"), Qt::CaseInsensitive) == 0) {
        return toIconv(text, "SHIFT-JIS");
    }
    if (encodingHint.compare(QStringLiteral("ISO-8859-1"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("Latin1"), Qt::CaseInsensitive) == 0) {
        return text.toLatin1();
    }
    return QByteArray();
}

int EncodingDetector::codePageId(const QString &encodingHint)
{
    if (encodingHint.compare(QStringLiteral("GBK"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("GB2312"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("GB18030"), Qt::CaseInsensitive) == 0) {
        return 936;
    }
    if (encodingHint.compare(QStringLiteral("Big5"), Qt::CaseInsensitive) == 0) {
        return 950;
    }
    if (encodingHint.compare(QStringLiteral("Shift-JIS"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("SJIS"), Qt::CaseInsensitive) == 0) {
        return 932;
    }
    if (encodingHint.compare(QStringLiteral("ISO-8859-1"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("Latin1"), Qt::CaseInsensitive) == 0) {
        return 28591;
    }
    return 0;
}

bool EncodingDetector::isLocaleAvailable(const QString &localeName)
{
#ifdef Q_OS_WIN
    Q_UNUSED(localeName);
    return true; // Windows always has the Unicode/legacy code page APIs.
#else
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
#endif
}
