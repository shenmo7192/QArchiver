#ifndef ARCHIVEGRIDVIEW_H
#define ARCHIVEGRIDVIEW_H

// QArchiver / 小Q解压
// File-manager-like icon grid for archive contents.
// 类似文件管理器的压缩包内容网格视图。

#include <QListView>
#include <QPoint>
#include <QStringList>

class QMouseEvent;

// Icon-mode view sharing FileListModel/FilterProxyModel with ArchiveTreeView.
// It starts the existing drag-out workflow instead of Qt's internal model drag.
// 与 ArchiveTreeView 共享 FileListModel/FilterProxyModel 的图标模式视图。
// 它启动既有的拖出流程，而不是 Qt 的模型内部拖拽。
class ArchiveGridView : public QListView {
    Q_OBJECT
public:
    explicit ArchiveGridView(QWidget *parent = nullptr);

    // Selected archive entry paths (full internal paths).
    // 选中的压缩包条目路径（完整内部路径）。
    QStringList selectedEntryPaths() const;

signals:
    void entryActivated(const QString &path);
    void dragOutRequested(const QStringList &paths);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QPoint m_dragStartPos;
    bool m_dragActive = false;
};

#endif // ARCHIVEGRIDVIEW_H
