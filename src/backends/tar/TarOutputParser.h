#ifndef TAROUTPUTPARSER_H
#define TAROUTPUTPARSER_H

#include <QList>
#include <QString>
#include <QDateTime>
#include "core/ArchiveEntry.h"

// Parses `tar -tv` (list) and `tar -xv` (extract) output (design doc 5.1.5)
class TarOutputParser {
public:
    // Parse a `-tv` verbose listing line into an ArchiveEntry
    // Format: "drwxr-xr-x owner/group  0 2024-01-01 12:00 path/to/entry"
    static ArchiveEntry parseListLine(const QString &line);

    // Parse all list lines
    static QList<ArchiveEntry> parseList(const QString &output);

    // Count processed entries from extract output (-v lists each entry as extracted)
    static int countProcessed(const QString &output);

    // Determine whether a list line indicates a directory
    static bool isDirectory(const QString &line);
};

#endif // TAROUTPUTPARSER_H
