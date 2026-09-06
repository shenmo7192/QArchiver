#include "BookmarksManager.h"

#include "AppSettings.h"

BookmarksManager::BookmarksManager() = default;

BookmarksManager &BookmarksManager::instance()
{
    static BookmarksManager s;
    return s;
}

void BookmarksManager::add(const QString &filePath)
{
    QStringList bms = list();
    if (!bms.contains(filePath)) {
        bms << filePath;
        AppSettings::instance().setBookmarks(bms);
        emit changed();
    }
}

void BookmarksManager::remove(const QString &filePath)
{
    QStringList bms = list();
    if (bms.removeAll(filePath) > 0) {
        AppSettings::instance().setBookmarks(bms);
        emit changed();
    }
}

QStringList BookmarksManager::list() const
{
    return AppSettings::instance().bookmarks();
}

bool BookmarksManager::contains(const QString &filePath) const
{
    return list().contains(filePath);
}
