#ifndef ZIPBACKEND_H
#define ZIPBACKEND_H

#include "backends/sevenzip/SevenZipBackend.h"

// Thin .zip specialization: all process handling and parsing is shared with
// SevenZipBackend; only the 7z format switch (-tzip) differs.
class ZipBackend : public SevenZipBackend {
    Q_OBJECT
public:
    explicit ZipBackend(QObject *parent = nullptr)
        : SevenZipBackend(ArchiveFormat::Zip, parent)
    {
    }

    ArchiveFormat format() const override { return ArchiveFormat::Zip; }
};

#endif // ZIPBACKEND_H
