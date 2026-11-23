// QArchiver / 小Q解压
// Minimal reader/writer for the freedesktop mimeapps.list format.
// freedesktop mimeapps.list 格式的最小读写实现。

#include "utils/MimeAppsList.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {

constexpr auto kDefaultSection = "Default Applications";
constexpr auto kAddedSection = "Added Associations";

QString sectionName(const QString &line)
{
    const QString trimmed = line.trimmed();
    if (trimmed.size() < 2 || !trimmed.startsWith(QLatin1Char('['))
        || !trimmed.endsWith(QLatin1Char(']'))) {
        return QString();
    }
    return trimmed.mid(1, trimmed.size() - 2).trimmed();
}

bool isSectionHeader(const QString &line)
{
    const QString trimmed = line.trimmed();
    return trimmed.size() >= 2 && trimmed.startsWith(QLatin1Char('['))
        && trimmed.endsWith(QLatin1Char(']'));
}

QStringList splitValues(const QString &value)
{
    QStringList values;
    for (const QString &part : value.split(QLatin1Char(';'))) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            values.append(trimmed);
        }
    }
    return values;
}

QString joinedValues(const QStringList &values)
{
    if (values.isEmpty()) {
        return QString();
    }
    return values.join(QLatin1Char(';')) + QLatin1Char(';');
}

bool updateSection(QStringList &lines, const QString &section,
                   const QString &mimeType, const QString &desktopId,
                   bool add)
{
    bool foundSection = false;

    for (int i = 0; i < lines.size(); ++i) {
        if (!isSectionHeader(lines.at(i))
            || sectionName(lines.at(i)) != section) {
            continue;
        }

        foundSection = true;
        int end = i + 1;
        while (end < lines.size() && !isSectionHeader(lines.at(end))) {
            ++end;
        }

        bool foundKey = false;
        for (int j = i + 1; j < end;) {
            const QString &line = lines.at(j);
            const int equals = line.indexOf(QLatin1Char('='));
            if (equals < 0 || line.left(equals).trimmed() != mimeType) {
                ++j;
                continue;
            }

            // A valid file has one assignment per MIME type.  If a broken file
            // contains duplicates, keep the first one and drop the rest.
            // 正常文件每个 MIME 类型只有一行；遇到损坏文件的重复行时保留第一行。
            if (foundKey) {
                lines.removeAt(j);
                --end;
                continue;
            }
            foundKey = true;

            QStringList values = splitValues(line.mid(equals + 1));
            values.removeAll(desktopId);
            if (add) {
                values.prepend(desktopId);
            }

            if (values.isEmpty()) {
                lines.removeAt(j);
                --end;
            } else {
                lines[j] = mimeType + QLatin1Char('=')
                    + joinedValues(values);
                ++j;
            }
        }

        if (!foundKey && add) {
            lines.insert(end, mimeType + QLatin1Char('=') + desktopId
                                  + QLatin1Char(';'));
        }

        i = end - 1;
    }

    if (!foundSection && add) {
        if (!lines.isEmpty() && !lines.last().trimmed().isEmpty()) {
            lines.append(QString());
        }
        lines.append(QLatin1Char('[') + section + QLatin1Char(']'));
        lines.append(mimeType + QLatin1Char('=') + desktopId
                     + QLatin1Char(';'));
    }

    return true;
}

bool readLines(const QString &filePath, QStringList *lines, QString *error)
{
    QFile file(filePath);
    if (!file.exists()) {
        lines->clear();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Could not read %1").arg(filePath);
        }
        return false;
    }

    QString content = QString::fromUtf8(file.readAll());
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    *lines = content.split(QLatin1Char('\n'));
    if (!lines->isEmpty() && lines->last().isEmpty()) {
        lines->removeLast();
    }
    return true;
}

bool writeLines(const QString &filePath, const QStringList &lines,
                QString *error)
{
    const QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error) {
            *error = QStringLiteral("Could not create %1")
                         .arg(info.absolutePath());
        }
        return false;
    }

    QString output = lines.join(QLatin1Char('\n'));
    if (!output.isEmpty()) {
        output += QLatin1Char('\n');
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("Could not write %1").arg(filePath);
        }
        return false;
    }
    if (file.write(output.toUtf8()) < 0 || !file.commit()) {
        if (error) {
            *error = QStringLiteral("Could not commit %1").arg(filePath);
        }
        return false;
    }
    return true;
}

} // namespace

namespace MimeAppsList {

QStringList desktopIds(const QString &filePath, const QString &section,
                       const QString &mimeType)
{
    QStringList lines;
    QString error;
    if (!readLines(filePath, &lines, &error)) {
        return {};
    }

    QStringList result;
    QString currentSection;
    for (const QString &line : lines) {
        if (isSectionHeader(line)) {
            currentSection = sectionName(line);
            continue;
        }
        if (currentSection != section) {
            continue;
        }
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals < 0 || line.left(equals).trimmed() != mimeType) {
            continue;
        }
        result = splitValues(line.mid(equals + 1));
        break;
    }
    return result;
}

bool hasDesktopId(const QString &filePath, const QString &section,
                  const QString &mimeType, const QString &desktopId)
{
    return desktopIds(filePath, section, mimeType).contains(desktopId);
}

bool setAssociation(const QString &filePath, const QString &mimeType,
                    const QString &desktopId, bool add, QString *error)
{
    if (mimeType.isEmpty() || desktopId.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Empty MIME type or desktop ID");
        }
        return false;
    }
    // Removing from a file that does not exist is already a no-op; avoid
    // creating an empty mimeapps.list on uninstall.
    // 文件不存在时移除本来就是空操作，避免卸载时创建空的 mimeapps.list。
    if (!add && !QFileInfo::exists(filePath)) {
        return true;
    }

    QStringList lines;
    if (!readLines(filePath, &lines, error)) {
        return false;
    }
    updateSection(lines, QString::fromLatin1(kDefaultSection), mimeType,
                  desktopId, add);
    updateSection(lines, QString::fromLatin1(kAddedSection), mimeType,
                  desktopId, add);
    return writeLines(filePath, lines, error);
}

} // namespace MimeAppsList
