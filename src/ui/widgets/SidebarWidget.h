#ifndef SIDEBARWIDGET_H
#define SIDEBARWIDGET_H

#include <QWidget>

class QListWidget;
class QListWidgetItem;

// Sidebar with the recently-opened archives list (design doc 5.9).
// The visible title is the QDockWidget title bar created by MainWindow, so the
// widget itself only hosts the list.
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
