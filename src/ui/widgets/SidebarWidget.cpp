// QArchiver / 小Q解压
// SidebarWidget item management and signals.
// SidebarWidget 的条目管理与信号。

#include "ui/widgets/SidebarWidget.h"

#include "core/RecentFilesManager.h"
#include "ui/icons/IconUtils.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QMenu>

SidebarWidget::SidebarWidget(QWidget *parent)
    : QWidget(parent)
{
    // The title is provided by the enclosing QDockWidget (MainWindow), so no
    // extra heading label is added here.
    // 标题由外层 QDockWidget（MainWindow）提供，因此这里不再添加额外标题标签。
    m_recentList = new QListWidget(this);
    m_recentList->setObjectName(QStringLiteral("RecentList"));
    m_recentList->setIconSize(QSize(18, 18));
    m_recentList->setContextMenuPolicy(Qt::CustomContextMenu);
    m_recentList->setToolTip(tr("Recently opened archives. Double-click to open."));

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 6, 4, 4);
    layout->setSpacing(0);
    layout->addWidget(m_recentList, 1);

    connect(m_recentList, &QListWidget::itemDoubleClicked, this, &SidebarWidget::onRecentClicked);

    connect(&RecentFilesManager::instance(), &RecentFilesManager::changed,
            this, &SidebarWidget::refreshRecent);

    // Context menu for the recent-files list / 最近文件列表的上下文菜单
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
            // 移除文件已不存在的条目（保持顺序）。
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

    refreshRecent();
}

void SidebarWidget::refreshRecent()
{
    m_recentList->clear();
    const QStringList paths = RecentFilesManager::instance().validatedList();
    for (const QString &path : paths) {
        auto *item = new QListWidgetItem(Icons::aero(QStringLiteral("Open.png")),
                                         QFileInfo(path).fileName(), m_recentList);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        if (!QFileInfo::exists(path)) {
            item->setForeground(Qt::gray);
        }
    }
}

void SidebarWidget::onRecentClicked(QListWidgetItem *item)
{
    if (item) {
        emit fileOpened(item->data(Qt::UserRole).toString());
    }
}
