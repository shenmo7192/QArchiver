#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>

class QListWidget;
class QListWidgetItem;

// Sidebar with Recent Files and Bookmarks (design doc 5.9)
class SidebarWidget : public QWidget {
    Q_OBJECT
public:
    explicit SidebarWidget(QWidget *parent = nullptr);

signals:
    void fileOpened(const QString &filePath);
    void bookmarkRemoved(const QString &filePath);

private slots:
    void refreshRecent();
    void refreshBookmarks();
    void onRecentClicked(QListWidgetItem *item);
    void onBookmarkClicked(QListWidgetItem *item);

private:
    QListWidget *m_recentList = nullptr;
    QListWidget *m_bookmarkList = nullptr;
};

#endif // SIDEBARWIDGET_H
