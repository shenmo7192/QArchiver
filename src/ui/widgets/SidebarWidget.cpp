#include "SidebarWidget.h"

#include "core/RecentFilesManager.h"
#include "core/BookmarksManager.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QLabel>
#include <QFileInfo>
#include <QMenu>

SidebarWidget::SidebarWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *recentLabel = new QLabel(tr("Recent Files"), this);
    m_recentList = new QListWidget(this);
    m_recentList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_recentList->setToolTip(tr("Recently opened archives. Double-click to open."));

    auto *bookmarkLabel = new QLabel(tr("Bookmarks"), this);
    m_bookmarkList = new QListWidget(this);
    m_bookmarkList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_bookmarkList->setToolTip(tr("Bookmarked archives. Double-click to open."));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->addWidget(recentLabel);
    layout->addWidget(m_recentList, 1);
    layout->addWidget(bookmarkLabel);
    layout->addWidget(m_bookmarkList, 1);

    connect(m_recentList, &QListWidget::itemDoubleClicked, this, &SidebarWidget::onRecentClicked);
    connect(m_bookmarkList, &QListWidget::itemDoubleClicked, this, &SidebarWidget::onBookmarkClicked);

    connect(&RecentFilesManager::instance(), &RecentFilesManager::changed,
            this, &SidebarWidget::refreshRecent);
    connect(&BookmarksManager::instance(), &BookmarksManager::changed,
            this, &SidebarWidget::refreshBookmarks);

    // Context menu for the recent-files list
    connect(m_recentList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        auto *item = m_recentList->itemAt(pos);
        auto *menu = new QMenu(this);

        if (item) {
            const QString path = item->data(Qt::UserRole).toString();
            menu->addAction(tr("Open"), this, [this, path]() { emit fileOpened(path); });
            menu->addAction(tr("Remove from list"), this, [path]() {
                RecentFilesManager::instance().remove(path);
            });
        }
        menu->addSeparator();
        menu->addAction(tr("Clear missing entries"), this, []() {
            // Remove entries whose files no longer exist (order preserved).
            const QStringList stored = RecentFilesManager::instance().list();
            for (const QString &p : stored) {
                if (!QFileInfo::exists(p)) {
                    RecentFilesManager::instance().remove(p);
                }
            }
        });
        menu->exec(m_recentList->viewport()->mapToGlobal(pos));
        menu->deleteLater();
    });

    // Context menu for the bookmarks list
    connect(m_bookmarkList, &QListWidget::customContextMenuRequested, this,
            [this](const QPoint &pos) {
        auto *item = m_bookmarkList->itemAt(pos);
        if (!item) {
            return;
        }
        auto *menu = new QMenu(this);
        const QString path = item->data(Qt::UserRole).toString();
        menu->addAction(tr("Open"), this, [this, path]() { emit fileOpened(path); });
        menu->addSeparator();
        menu->addAction(tr("Remove bookmark"), this, [this, path]() {
            BookmarksManager::instance().remove(path);
            emit bookmarkRemoved(path);
        });
        menu->exec(m_bookmarkList->viewport()->mapToGlobal(pos));
        menu->deleteLater();
    });

    refreshRecent();
    refreshBookmarks();
}

void SidebarWidget::refreshRecent()
{
    m_recentList->clear();
    const QStringList paths = RecentFilesManager::instance().validatedList();
    for (const QString &path : paths) {
        auto *item = new QListWidgetItem(QFileInfo(path).fileName(), m_recentList);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        if (!QFileInfo::exists(path)) {
            item->setForeground(Qt::gray);
        }
    }
}

void SidebarWidget::refreshBookmarks()
{
    m_bookmarkList->clear();
    const QStringList paths = BookmarksManager::instance().list();
    for (const QString &path : paths) {
        auto *item = new QListWidgetItem(QFileInfo(path).fileName(), m_bookmarkList);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }
}

void SidebarWidget::onRecentClicked(QListWidgetItem *item)
{
    if (item) {
        emit fileOpened(item->data(Qt::UserRole).toString());
    }
}

void SidebarWidget::onBookmarkClicked(QListWidgetItem *item)
{
    if (item) {
        emit fileOpened(item->data(Qt::UserRole).toString());
    }
}
