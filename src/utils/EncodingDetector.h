#ifndef ENCODINGDETECTOR_H
#define ENCODINGDETECTOR_H

// QArchiver / 小Q解压
// Heuristic filename encoding detection and conversion helpers.
// 文件名编码启发式检测与转换辅助函数。

#include <QByteArray>
#include <QString>

// Heuristic encoding detector for filename output (design doc 5.1.6).
//
// 7-Zip on Windows emits UTF-8 when invoked with -sccUTF-8.  On Linux, however,
// legacy ZIPs without the UTF-8 flag are passed through as raw bytes, so the
// 7z backends still rely on this detector for those archives.
//
// 文件名输出的启发式编码检测器（设计文档 5.1.6）。
// Windows 上使用 -sccUTF-8 的 7-Zip 会输出 UTF-8；但在 Linux 上，
// 没有 UTF-8 标志的旧式 ZIP 会以原始字节输出，因此 7z 后端仍需该检测器。
class EncodingDetector {
public:
    // Detect whether the byte sequence is likely UTF-8, GB18030, Big5 or Shift-JIS.
    // Returns the detected encoding name, or "ISO-8859-1" as the last resort.
    // 检测字节序列更可能属于 UTF-8、GB18030、Big5 还是 Shift-JIS。
    // 返回检测到的编码名，最后兜底为 "ISO-8859-1"。
    static QString detect(const QByteArray &data);

    // Decode bytes using the detected or specified encoding.
    // 使用检测到或指定的编码解码字节。
    static QString decode(const QByteArray &data, const QString &encodingHint = QString());

    // Encode text back to the archive's legacy encoding.  Used for 7-Zip
    // listfiles (-scs<codepage>) when extracting selected members from a ZIP
    // whose names are not UTF-8.  Returns an empty QByteArray if the text
    // cannot be represented in the requested encoding.
    // 将文本编码回压缩包的旧编码，用于从非 UTF-8 名称的 ZIP 中解压选定条目时
    // 生成 7-Zip 列表文件（-scs<代码页>）。若文本无法用目标编码表示，
    // 返回空 QByteArray。
    static QByteArray encode(const QString &text, const QString &encodingHint);

    // Map a detected encoding name to the Windows code page accepted by
    // 7-Zip's -scs{id} switch.  Returns 0 when the encoding has no mapping.
    // 将检测到的编码名映射为 7-Zip -scs{id} 接受的 Windows 代码页。
    // 没有对应映射时返回 0。
    static int codePageId(const QString &encodingHint);

    // Check whether a system locale is available (e.g. zh_CN.GBK).
    // 检查系统区域设置是否可用（例如 zh_CN.GBK）。
    static bool isLocaleAvailable(const QString &localeName);
};

#endif // ENCODINGDETECTOR_H
