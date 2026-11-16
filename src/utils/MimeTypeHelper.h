#ifndef MIMETYPEHELPER_H
#define MIMETYPEHELPER_H

#include <QString>
#include <QByteArray>

#include <cstdint>
#include <span>

#include "ArchiveConstants.h"

// Format detection authority (design doc 4.3)
// Three-stage detection: Magic Number -> tar stream probe -> suffix fallback
class MimeTypeHelper {
public:
    // Main entry: returns detected format; Unknown if unrecognizable
    static ArchiveFormat detect(const QString &filePath);

    // Read file head bytes (default 512 bytes, covers all target formats)
    static QByteArray readMagic(const QString &filePath, int maxBytes = 512);

    // Match a signature at a given offset
    static bool matchMagic(const QByteArray &head, std::span<const std::uint8_t> signature, int offset);

    // Extract lowercase extension (normalizes .tgz/.txz/.tbz2)
    static QString extensionOf(const QString &filePath);

    // Check if the archive is a split volume (e.g. .7z.001, .zip.001)
    static bool isSplitVolume(const QString &filePath);

    // Resolve the first volume path from any volume file
    static QString resolveFirstVolume(const QString &filePath);
};

#endif // MIMETYPEHELPER_H
