// QArchiver / 小Q解压
// UTF-8/GB18030/Big5/Shift-JIS detection, decoding and code-page mapping.
// UTF-8/GB18030/Big5/Shift-JIS 检测、解码及代码页映射。

#include "utils/EncodingDetector.h"

#include <QLocale>
#include <QProcess>
#include <QStringConverter>
#include <QStringList>

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

// iconv name for the legacy encodings supported by the detector, or nullptr.
// 检测器支持的旧编码对应的 iconv 名称，不支持时返回 nullptr。
const char *iconvNameFor(const QString &encodingHint)
{
    if (encodingHint.compare(QStringLiteral("GBK"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("GB2312"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("GB18030"), Qt::CaseInsensitive) == 0) {
        return "GB18030";
    }
    if (encodingHint.compare(QStringLiteral("Big5"), Qt::CaseInsensitive) == 0) {
        return "BIG5";
    }
    if (encodingHint.compare(QStringLiteral("Shift-JIS"), Qt::CaseInsensitive) == 0
        || encodingHint.compare(QStringLiteral("SJIS"), Qt::CaseInsensitive) == 0) {
        return "SHIFT-JIS";
    }
    return nullptr;
}

// Legacy candidates for detect(), biased by the system locale.  The bias only
// decides *ties*: candidates are ranked by how plausible their interpretation
// of the bytes is (see legacyPlausibility), because GB18030 is a superset of
// GBK and accepts almost any byte pair, so "first decodable wins" would
// swallow every Big5 name.
// detect() 的旧编码候选，按系统 locale 排列。locale 只决定*平票*：
// 候选按“其解释有多可信”排序（见 legacyPlausibility），
// 因为 GB18030 是 GBK 的超集、几乎能解码任意字节对，
// “谁先解码成功就算谁”会把所有 Big5 名称吞成 GB18030。
const QStringList &legacyEncodingOrder()
{
    static const QStringList order = [] {
        QStringList candidates;
        const QString locale = QLocale::system().name();
        if (locale.startsWith(QLatin1String("zh_TW"))
            || locale.startsWith(QLatin1String("zh_HK"))
            || locale.startsWith(QLatin1String("zh_Hant"))) {
            candidates << QStringLiteral("Big5");
        } else if (locale.startsWith(QLatin1String("ja"))) {
            candidates << QStringLiteral("Shift-JIS");
        }
        for (const QString &encoding : {QStringLiteral("GB18030"),
                                        QStringLiteral("Big5"),
                                        QStringLiteral("Shift-JIS")}) {
            if (!candidates.contains(encoding)) {
                candidates << encoding;
            }
        }
        return candidates;
    }();
    return order;
}

// Minimum score gap between the two best interpretations before the ranking
// is trusted; anything closer is treated as ambiguous and resolved by locale.
// 两个最优解释的最小分差；小于它则视为歧义，交给 locale 顺序裁决。
constexpr double kPlausibilityMargin = 0.05;

// Byte ranges of the "frequently used" block of each legacy encoding: GB2312
// level-1 hanzi, the Big5 frequently used block, and the Shift-JIS kana plus
// JIS X 0208 level-1 kanji.
// 各旧编码“常用字区”的字节范围：GB2312 一级汉字、Big5 常用字区，
// 以及 Shift-JIS 假名与 JIS X 0208 第一水准汉字。
//
// score = the share of two-byte code units that fall into that block.  The
// walk works directly on the raw bytes (iconv is only used to prove the bytes
// are decodable at all): a two-byte code unit re-encodes to the very same
// bytes, so its position in the code page is known without decoding character
// by character.  A four-byte GB18030 form means a rare ideograph and scores as
// uncommon.
// 分数 = 落入该区间的两字节代码单元比例。扫描直接作用于原始字节
// （iconv 只用来确认这串字节确实可解码）：两字节代码单元重新编码后仍是同样的
// 字节，因此无需逐字解码就能知道它在码表中的位置。
// 四字节 GB18030 形式代表罕用字，计为不常用。
//
// Returns -1 when the bytes are not decodable with this encoding, or when the
// sample carries no wide characters at all.
// 若这些字节无法用该编码解码，或样本里根本没有宽字符，返回 -1。
double legacyPlausibility(const QByteArray &data, const QString &encoding)
{
    const char *iconvName = iconvNameFor(encoding);
    if (!iconvName || data.isEmpty()) {
        return -1.0;
    }
    if (fromIconv(data, iconvName).isEmpty()) {
        return -1.0;
    }

    const bool gb18030 = encoding.compare(QStringLiteral("GB18030"), Qt::CaseInsensitive) == 0;
    const bool big5 = encoding.compare(QStringLiteral("Big5"), Qt::CaseInsensitive) == 0;
    const auto *bytes = reinterpret_cast<const quint8 *>(data.constData());
    const int size = data.size();

    int considered = 0;
    int common = 0;
    int index = 0;
    while (index < size) {
        const quint8 lead = bytes[index];
        if (lead < 0x80) {
            ++index; // ASCII carries no signal.
            continue;
        }
        if (!gb18030 && !big5 && lead >= 0xA1 && lead <= 0xDF) {
            ++considered; // Shift-JIS half-width katakana occupy one byte.
            ++index;
            continue;
        }
        if (index + 1 >= size) {
            break;
        }
        const quint8 trail = bytes[index + 1];
        ++considered;
        if (gb18030) {
            if (trail >= 0x30 && trail <= 0x39 && index + 3 < size) {
                index += 4;
                continue;
            }
            if (lead >= 0xB0 && lead <= 0xF7 && trail >= 0xA1 && trail <= 0xFE) {
                ++common;
            }
            index += 2;
            continue;
        }
        if (big5) {
            if (lead >= 0xA4 && lead <= 0xC6) {
                ++common;
            }
            index += 2;
            continue;
        }
        if ((lead >= 0x82 && lead <= 0x83) || (lead >= 0x88 && lead <= 0x98)) {
            ++common;
        }
        index += 2;
    }

    if (considered == 0) {
        return -1.0;
    }
    return static_cast<double>(common) / considered;
}

QString decodeWithIconv(const QByteArray &data, const QString &encodingHint)
{
    if (const char *iconvName = iconvNameFor(encodingHint)) {
        return fromIconv(data, iconvName);
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

    // UTF-8 always wins when it is valid (checked above).  Among the legacy
    // encodings the most plausible interpretation wins; the locale order is
    // only the tie-breaker.
    // UTF-8 合法时总是优先（已在上面判定）。
    // 旧编码之间取“最像”的那一种；locale 顺序仅用于平票裁决。
    const QStringList &order = legacyEncodingOrder();

    double bestScore = -1.0;
    double secondScore = -1.0;
    QString best;
    QStringList decodable; // Kept in locale order for tie-breaking.
    for (const QString &encoding : order) {
        const double score = legacyPlausibility(data, encoding);
        if (score < 0.0) {
            continue;
        }
        decodable << encoding;
        if (score > bestScore) {
            secondScore = bestScore;
            bestScore = score;
            best = encoding;
        } else if (score > secondScore) {
            secondScore = score;
        }
    }

    if (best.isEmpty()) {
        return QStringLiteral("ISO-8859-1");
    }
    // Near-ties happen where the byte ranges overlap (a two-character Big5 name
    // may be plausible as both); there the locale decides.
    // 字节范围重叠处会出现近似平票（两个字的 Big5 名称在两种编码下都说得通），
    // 此时交给 locale 裁决。
    if (bestScore - secondScore < kPlausibilityMargin) {
        return decodable.first();
    }
    return best;
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
