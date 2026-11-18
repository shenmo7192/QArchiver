#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

// QArchiver / 小Q解压
// Sidebar with recent files and bookmarks.
// 包含最近文件与书签的侧边栏。

#include <QWidget>

class QListWidget;
class QListWidgetItem;

// Sidebar with the recently-opened archives list (design doc 5.9).
// The visible title is the QDockWidget title bar created by MainWindow, so the
// widget itself only hosts the list.
// 显示最近打开压缩包列表的侧边栏（设计文档 5.9）。
// 可见标题由 MainWindow 创建的 QDockWidget 标题栏提供，
// 因此该控件本身只承载列表。
class SidebarWidget : public QWidget {
    Q_OBJECT
public:
    explicit SidebarWidget(QWidget *parent = nullptr);

signals:
    void fileOpened(const QString &filePath);

private slots:
    void refreshRecent();
    void onRecentClicked(QListWidgetItem *item);

private:
    QListWidget *m_recentList = nullptr;
};

#endif // SIDEBARWIDGET_H
