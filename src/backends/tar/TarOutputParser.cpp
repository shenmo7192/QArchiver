#include "TarOutputParser.h"

#include <QStringList>
#include <QRegularExpression>
#include <QTextStream>

ArchiveEntry TarOutputParser::parseListLine(const QString &line)
{
    ArchiveEntry entry;
    if (line.isEmpty()) {
        return entry;
    }

    // GNU tar -tv format: "perms owner/group size date time path"
    // Example: "drwxr-xr-x user/group     0 2024-01-01 12:00 path"
    // The path may contain spaces, so parse from the front
    const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    if (parts.size() < 6) {
        // Might be just a path from -v extract output
        entry.path = line.trimmed();
        entry.isDir = entry.path.endsWith('/');
        return entry;
    }

    // parts[0] = permissions (e.g. drwxr-xr-x)
    const QString &perms = parts.at(0);
    entry.isDir = perms.startsWith('d');
    entry.isSymLink = perms.startsWith('l');

    // parts[1] = owner/group
    // parts[2] = size
    bool ok = false;
    entry.size = parts.at(2).toLongLong(&ok);

    // parts[3] = date (YYYY-MM-DD), parts[4] = time (HH:MM)
    if (parts.size() >= 5) {
        QDateTime dt = QDateTime::fromString(parts.at(3) + ' ' + parts.at(4),
                                             QStringLiteral("yyyy-MM-dd HH:mm"));
        if (dt.isValid()) {
            entry.mtime = dt;
        }
    }

    // Path: everything after the 5th field
    int pathIdx = 0;
    int spaceCount = 0;
    for (int i = 0; i < line.size(); ++i) {
        if (line.at(i).isSpace()) {
            // Skip consecutive whitespace
            if (i > 0 && !line.at(i - 1).isSpace()) {
                ++spaceCount;
            }
            if (spaceCount == 5) {
                pathIdx = i;
                while (pathIdx < line.size() && line.at(pathIdx).isSpace()) {
                    ++pathIdx;
                }
                break;
            }
        }
    }
    if (pathIdx > 0 && pathIdx < line.size()) {
        entry.path = line.mid(pathIdx).trimmed();
    } else {
        entry.path = parts.last();
    }

    // If symlink, split "link -> target"
    if (entry.isSymLink && entry.path.contains(QStringLiteral(" -> "))) {
        entry.path = entry.path.section(QStringLiteral(" -> "), 0, 0);
    }
    // Strip trailing '/' for directories (keep path clean)
    while (entry.path.endsWith('/')) {
        entry.path.chop(1);
    }

    if (!ok) {
        entry.size = 0;
    }
    return entry;
}

QList<ArchiveEntry> TarOutputParser::parseList(const QString &output)
{
    QList<ArchiveEntry> entries;
    QString data = output;
    QTextStream stream(&data);
    stream.setEncoding(QStringConverter::Utf8);
    QString line;
    while (stream.readLineInto(&line)) {
        if (line.isEmpty()) continue;
        ArchiveEntry e = parseListLine(line);
        if (!e.path.isEmpty()) {
            entries.append(e);
        }
    }
    return entries;
}

int TarOutputParser::countProcessed(const QString &output)
{
    int count = 0;
    QString data = output;
    QTextStream stream(&data);
    stream.setEncoding(QStringConverter::Utf8);
    QString line;
    while (stream.readLineInto(&line)) {
        if (!line.isEmpty()) {
            ++count;
        }
    }
    return count;
}

bool TarOutputParser::isDirectory(const QString &line)
{
    return !line.isEmpty() && line.at(0) == 'd';
}
