// QArchiver / 小Q解压
// RecentFilesManager implementation and stale-entry cleanup.
// RecentFilesManager 实现及失效条目清理。

#include "RecentFilesManager.h"

#include "AppSettings.h"
#include <QFileInfo>

RecentFilesManager::RecentFilesManager() = default;

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
    while (files.size() > 20) {
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
