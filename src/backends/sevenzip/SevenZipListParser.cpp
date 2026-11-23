// QArchiver / 小Q解压
// Parses 7-Zip -slt blocks into ArchiveEntry values.
// 将 7-Zip -slt 块解析为 ArchiveEntry。

#include "backends/sevenzip/SevenZipListParser.h"

#include <QDateTime>

QList<ArchiveEntry> SevenZipListParser::parse(const QString &output)
{
    SevenZipListParser parser;
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    for (const QString &line : lines) {
        parser.feedLine(line);
    }
    return parser.takeEntries();
}

void SevenZipListParser::feedLine(const QString &rawLine)
{
    QString line = rawLine;
    if (line.endsWith(QLatin1Char('\r'))) {
        line.chop(1);
    }

    const int equals = line.indexOf(QLatin1Char('='));
    if (equals < 0) {
        return;
    }
    const QString key = line.left(equals).trimmed();

    // Only remove the single delimiter space after '='.  Member names may
    // legally begin or end with spaces, so trimming the whole value would
    // corrupt them.
    // 仅去掉 '=' 后的一个分隔空格。成员名可以合法地以空格开头或结尾，
    // 对整个值 trim 会破坏这些名称。
    QString value = line.mid(equals + 1);
    if (value.startsWith(QLatin1Char(' '))) {
        value.remove(0, 1);
    }

    if (key == QLatin1String("Path")) {
        commitEntry();
        m_current.path = value;
#ifdef Q_OS_WIN
        // 7-Zip uses backslashes for member paths on Windows; the rest of
        // the application (and the ZIP path checks) use forward slashes.
        // A backslash is a legal character in Unix filenames, so only
        // normalize on Windows.
        // Windows 上 7-Zip 使用反斜杠作为成员路径分隔符，而应用其他部分
        // （以及 ZIP 路径检查）使用正斜杠。反斜杠在 Unix 文件名中合法，
        // 因此只在 Windows 上做规范化。
        m_current.path.replace(QLatin1Char('\\'), QLatin1Char('/'));
#endif
    } else if (key == QLatin1String("Folder")) {
        m_current.isDir = (value.trimmed() == QLatin1String("+"));
        m_currentIsEntry = true;
    } else if (key == QLatin1String("Attributes")) {
        const QString trimmed = value.trimmed();
        m_current.isDir = trimmed.startsWith(QLatin1Char('D'));
        m_current.isSymLink = trimmed.startsWith(QLatin1Char('l'));
        m_currentIsEntry = true;
    } else if (key == QLatin1String("Mode")) {
        // tar (and some other formats) report a Unix mode string such as
        // "-rw-r--r--", "drwxr-xr-x" or "lrwxrwxrwx".
        // tar（及部分其他格式）会报告 Unix 权限字符串，例如
        // "-rw-r--r--"、"drwxr-xr-x" 或 "lrwxrwxrwx"。
        if (!value.isEmpty()) {
            m_current.isDir = value.startsWith(QLatin1Char('d'));
            m_current.isSymLink = value.startsWith(QLatin1Char('l'));
        }
        m_currentIsEntry = true;
    } else if (key == QLatin1String("Symbolic Link")) {
        if (!value.isEmpty()) {
            m_current.isSymLink = true;
            m_current.linkTarget = value;
        }
    } else if (key == QLatin1String("Hard Link")) {
        if (!value.isEmpty()) {
            m_current.isHardLink = true;
            m_current.hardLinkTarget = value;
        }
    } else if (key == QLatin1String("Size")) {
        m_current.size = value.trimmed().toLongLong();
    } else if (key == QLatin1String("Packed Size")) {
        m_current.packedSize = value.trimmed().toLongLong();
    } else if (key == QLatin1String("Modified")) {
        const QString trimmed = value.trimmed();
        m_current.mtime = QDateTime::fromString(trimmed.left(19),
                                                QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (!m_current.mtime.isValid()) {
            m_current.mtime = QDateTime::fromString(trimmed.left(16),
                                                    QStringLiteral("yyyy-MM-dd HH:mm"));
        }
    } else if (key == QLatin1String("Encrypted")) {
        m_current.isEncrypted = (value.trimmed() == QLatin1String("+"));
    }
}

void SevenZipListParser::commitEntry()
{
    if (m_currentIsEntry && !m_current.path.isEmpty()) {
        m_entries.append(m_current);
    }
    m_current = ArchiveEntry{};
    m_currentIsEntry = false;
}

QList<ArchiveEntry> SevenZipListParser::takeEntries()
{
    commitEntry();
    QList<ArchiveEntry> entries;
    entries.swap(m_entries);
    return entries;
}
