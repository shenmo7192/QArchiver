#ifndef ARCHIVEGRIDVIEW_H
#define ARCHIVEGRIDVIEW_H

// QArchiver / 小Q解压
// File-manager-like icon grid for archive contents.
// 类似文件管理器的压缩包内容网格视图。

#include <QListView>
#include <QPoint>
#include <QPointer>
#include <QStringList>

class QAbstractItemModel;
class QMouseEvent;
class QWheelEvent;

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

    // Recompute the cell height from the longest wrapped file name.
    // 按最长换行文件名重新计算单元格高度。
    void updateGridHeight();

    void setModel(QAbstractItemModel *model) override;

signals:
    void entryActivated(const QString &path);
    void dragOutRequested(const QStringList &paths);
    void zoomRequested(int direction);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    // Coalesce expensive height scans: thumbnail completion emits one
    // dataChanged per image and must not trigger a full QTextLayout sweep each
    // time. / 合并昂贵的高度扫描：缩略图完成会逐张发 dataChanged，不能每次都
    // 全量测量文件名。
    void scheduleGridHeightUpdate();

    QPoint m_dragStartPos;
    bool m_dragActive = false;
    bool m_gridHeightUpdatePending = false;
    QPointer<QAbstractItemModel> m_connectedModel;
};

#endif // ARCHIVEGRIDVIEW_H
