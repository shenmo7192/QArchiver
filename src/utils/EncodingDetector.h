#ifndef ENCODINGDETECTOR_H
#define ENCODINGDETECTOR_H

#include <QByteArray>
#include <QString>

// Heuristic encoding detector for tar/unrar filename output (design doc 5.1.6)
// 7z backends output UTF-8 automatically and do NOT use this detector.
class EncodingDetector {
public:
    // Detect whether the byte sequence is likely UTF-8, GB18030, Big5 or Shift-JIS.
    // Returns the detected encoding name, or "ISO-8859-1" as the last resort.
    static QString detect(const QByteArray &data);

    // Decode bytes using the detected or specified encoding.
    static QString decode(const QByteArray &data, const QString &encodingHint = QString());

    // Check whether a system locale is available (e.g. zh_CN.GBK).
    static bool isLocaleAvailable(const QString &localeName);
};

#endif // ENCODINGDETECTOR_H
