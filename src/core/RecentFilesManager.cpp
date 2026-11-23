// QArchiver / 小Q解压
// RecentFilesManager implementation and stale-entry cleanup.
// RecentFilesManager 实现及失效条目清理。

#include "core/RecentFilesManager.h"

#include "core/AppSettings.h"
#include <QFileInfo>

RecentFilesManager::RecentFilesManager()
{
    connect(&AppSettings::instance(), &AppSettings::recentFilesLimitChanged,
            this, &RecentFilesManager::trimToLimit);
}

RecentFilesManager &RecentFilesManager::instance()
{
    static RecentFilesManager s;
    return s;
}

void RecentFilesManager::add(const QString &filePath)
{
    QStringList files = list();
    // Dedup: move to front if exists / 去重：已存在则移到最前
    files.removeAll(filePath);
    files.prepend(filePath);
    const int limit = AppSettings::instance().recentFilesLimit();
    while (files.size() > limit) {
        files.removeLast();
    }
    AppSettings::instance().setRecentFiles(files);
    emit changed();
}

QStringList RecentFilesManager::list() const
{
    return AppSettings::instance().recentFiles();
}

void RecentFilesManager::remove(const QString &filePath)
{
    QStringList files = list();
    if (files.removeAll(filePath) > 0) {
        AppSettings::instance().setRecentFiles(files);
        emit changed();
    }
}

void RecentFilesManager::clear()
{
    AppSettings::instance().setRecentFiles({});
    emit changed();
}

void RecentFilesManager::trimToLimit()
{
    const QStringList files = list();
    const int limit = AppSettings::instance().recentFilesLimit();
    if (files.size() > limit) {
        AppSettings::instance().setRecentFiles(files.mid(0, limit));
    }
    emit changed();
}

QStringList RecentFilesManager::validatedList() const
{
    QStringList valid;
    for (const QString &path : list()) {
        if (QFileInfo::exists(path)) {
            valid << path;
        }
    }
    return valid;
}
