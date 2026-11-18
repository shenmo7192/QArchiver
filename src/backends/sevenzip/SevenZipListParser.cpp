// QArchiver / 小Q解压
// Parses 7-Zip -slt blocks into ArchiveEntry values.
// 将 7-Zip -slt 块解析为 ArchiveEntry。

#include "SevenZipListParser.h"

#include <QDateTime>
#include <QTextStream>

QList<ArchiveEntry> SevenZipListParser::parse(const QString &output)
{
    // The archive header block and each entry are "Key = Value" line groups;
    // a new entry starts with "Path =".  A block is an actual entry when it
    // contains "Folder" (zip/tar/rar), "Attributes" (7z) or "Mode" (tar).
    // 压缩包头块与每个条目都是 "Key = Value" 行组，新条目以 "Path =" 开始。
    // 只有包含 "Folder"（zip/tar/rar）、"Attributes"（7z）或 "Mode"（tar）
    // 的块才是真正条目。
    QList<ArchiveEntry> entries;
    QString data = output;
    QTextStream stream(&data);
    stream.setEncoding(QStringConverter::Utf8);
    QString line;
    ArchiveEntry current;
    bool currentIsEntry = false;

    auto commitEntry = [&]() {
        if (currentIsEntry && !current.path.isEmpty()) {
            entries.append(current);
        }
        current = ArchiveEntry{};
        currentIsEntry = false;
    };

    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals < 0) {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();

        if (key == QLatin1String("Path")) {
            commitEntry();
            current.path = value;
#ifdef Q_OS_WIN
            // 7-Zip uses backslashes for member paths on Windows; the rest of
            // the application (and the ZIP path checks) use forward slashes.
            // A backslash is a legal character in Unix filenames, so only
            // normalize on Windows.
            // Windows 上 7-Zip 使用反斜杠作为成员路径分隔符，而应用其他部分
            // （以及 ZIP 路径检查）使用正斜杠。反斜杠在 Unix 文件名中合法，
            // 因此只在 Windows 上做规范化。
            current.path.replace(QLatin1Char('\\'), QLatin1Char('/'));
#endif
        } else if (key == QLatin1String("Folder")) {
            current.isDir = (value == QLatin1String("+"));
            currentIsEntry = true;
        } else if (key == QLatin1String("Attributes")) {
            current.isDir = value.startsWith(QLatin1Char('D'));
            current.isSymLink = value.startsWith(QLatin1Char('l'));
            currentIsEntry = true;
        } else if (key == QLatin1String("Mode")) {
            // tar (and some other formats) report a Unix mode string such as
            // "-rw-r--r--", "drwxr-xr-x" or "lrwxrwxrwx".
            // tar（及部分其他格式）会报告 Unix 权限字符串，例如
            // "-rw-r--r--"、"drwxr-xr-x" 或 "lrwxrwxrwx"。
            if (!value.isEmpty()) {
                current.isDir = value.startsWith(QLatin1Char('d'));
                current.isSymLink = value.startsWith(QLatin1Char('l'));
            }
            currentIsEntry = true;
        } else if (key == QLatin1String("Symbolic Link")) {
            if (!value.isEmpty()) {
                current.isSymLink = true;
            }
        } else if (key == QLatin1String("Size")) {
            current.size = value.toLongLong();
        } else if (key == QLatin1String("Packed Size")) {
            current.packedSize = value.toLongLong();
        } else if (key == QLatin1String("Modified")) {
            current.mtime = QDateTime::fromString(value.left(19),
                                                  QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            if (!current.mtime.isValid()) {
                current.mtime = QDateTime::fromString(value.left(16),
                                                      QStringLiteral("yyyy-MM-dd HH:mm"));
            }
        } else if (key == QLatin1String("Encrypted")) {
            current.isEncrypted = (value == QLatin1String("+"));
        }
    }
    commitEntry();
    return entries;
}
