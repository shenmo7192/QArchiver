// QArchiver / 小Q解压
// Continuous-preview image selection.
// 连续看图的图片选取。

#include "PreviewUtils.h"

#include "utils/FileUtils.h"

#include <QSet>

namespace {

QString parentDirectoryOfNormalized(const QString &normalized)
{
    const int slash = normalized.lastIndexOf(QLatin1Char('/'));
    return slash < 0 ? QString() : normalized.left(slash);
}

} // namespace

namespace PreviewUtils {

bool isSupportedImagePath(const QString &path)
{
    static const QStringList suffixes = {
        QStringLiteral(".jpg"),
        QStringLiteral(".png"),
        QStringLiteral(".svg"),
        QStringLiteral(".webp"),
        QStringLiteral(".gif"),
        QStringLiteral(".bmp")
    };
    for (const QString &suffix : suffixes) {
        if (path.endsWith(suffix, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool shouldShowPreparationDialog(int imageCount, int threshold, bool allCached)
{
    if (allCached || imageCount <= 0) {
        return false;
    }
    return threshold == 0 || imageCount > threshold;
}

QString parentDirectory(const QString &entryPath)
{
    return parentDirectoryOfNormalized(FileUtils::normalizeEntryPath(entryPath));
}

QStringList imageSiblings(const QList<ArchiveEntry> &entries,
                          const QString &entryPath)
{
    const QString normalizedTarget = FileUtils::normalizeEntryPath(entryPath);
    if (!isSupportedImagePath(normalizedTarget)) {
        return {};
    }

    const QString targetDir = parentDirectoryOfNormalized(normalizedTarget);
    QStringList paths;
    QSet<QString> seen;

    for (const ArchiveEntry &entry : entries) {
        if (entry.isDir || entry.isSymLink) {
            continue;
        }

        const QString normalized = FileUtils::normalizeEntryPath(entry.path);
        if (normalized.isEmpty()
            || parentDirectoryOfNormalized(normalized) != targetDir
            || !isSupportedImagePath(normalized)
            || seen.contains(normalized)) {
            continue;
        }

        seen.insert(normalized);
        paths << entry.path;
    }

    // Keep the clicked image even if a backend's listing omitted it.
    // 即使后端列表漏掉点击项，也保证当前图片会被解压。
    if (!seen.contains(normalizedTarget)) {
        paths.prepend(entryPath);
    }
    return paths;
}

} // namespace PreviewUtils
