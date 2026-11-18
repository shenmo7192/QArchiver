// QArchiver / 小Q解压
// ArchiveTreeView sizing, expansion and selection helpers.
// ArchiveTreeView 的尺寸、展开与选择辅助逻辑。

#include "ArchiveTreeView.h"
#include "FileListModel.h"
#include "utils/Logger.h"

#include <QMouseEvent>
#include <QApplication>
#include <QItemSelectionModel>
#include <QStringList>

ArchiveTreeView::ArchiveTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setAlternatingRowColors(true);
    setSortingEnabled(true);
    setExpandsOnDoubleClick(false);

    connect(this, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const bool isDir = index.data(FileListModel::IsDirRole).toBool();
        if (!m_flatMode) {
            if (isDir || model()->rowCount(index) > 0) {
                setExpanded(index, !isExpanded(index));
                return;
            }
        }
        // In list mode directories (including the synthetic ".." row) are
        // emitted too; MainWindow performs the navigation.
        // 列表模式下目录（包括合成的 ".." 行）也会被触发；
        // 导航由 MainWindow 执行。
        const QString path = index.data(FileListModel::PathRole).toString();
        if (!path.isEmpty()) {
            emit entryDoubleClicked(path);
        }
    });
}

void ArchiveTreeView::setFlatMode(bool flat)
{
    m_flatMode = flat;
    setRootIsDecorated(!flat);
    setItemsExpandable(!flat);
    setIndentation(flat ? 0 : 20);
    if (flat) {
        collapseAll();
    }
}

QStringList ArchiveTreeView::selectedEntryPaths() const
{
    QStringList paths;
    const QModelIndexList rows = selectionModel()->selectedRows();
    for (const QModelIndex &idx : rows) {
        if (idx.data(FileListModel::IsParentRole).toBool()) {
            continue;
        }
        const QString path = idx.data(FileListModel::PathRole).toString();
        if (!path.isEmpty()) {
            paths << path;
        }
    }
    return paths;
}

void ArchiveTreeView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
    }
    QTreeView::mousePressEvent(event);
}

void ArchiveTreeView::mouseMoveEvent(QMouseEvent *event)
{
    // While the receiver prepares the payload and runs the native drag loop,
    // further mouse-move events are still delivered (the preparation spins a
    // nested event loop).  They must not start a second preparation.
    // 接收方准备拖拽数据并运行原生拖拽循环时，仍会收到鼠标移动事件
    // （准备过程运行嵌套事件循环）。这些事件不能启动第二次准备。
    if (m_dragActive) {
        return;
    }

    if (!(event->buttons() & Qt::LeftButton)) {
        return QTreeView::mouseMoveEvent(event);
    }

    // Only begin a drag when the pointer has moved far enough from a valid
    // entry; otherwise pass the event to the base class for normal handling.
    // 仅当指针从有效条目移动足够远时才开始拖拽；否则交给基类正常处理。
    if ((event->pos() - m_dragStartPos).manhattanLength()
        < QApplication::startDragDistance()) {
        return QTreeView::mouseMoveEvent(event);
    }

    const QModelIndex index = indexAt(m_dragStartPos);
    if (!index.isValid()) {
        LOG_DEBUG("ArchiveTreeView", "drag threshold passed but no entry at press pos");
        return QTreeView::mouseMoveEvent(event);
    }

    LOG_DEBUG("ArchiveTreeView", "drag threshold passed, starting drag");
    m_dragActive = true;
    startDrag(Qt::CopyAction);
    m_dragActive = false;
}

void ArchiveTreeView::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);

    const QStringList paths = selectedEntryPaths();
    LOG_INFO("ArchiveTreeView", "startDrag requested for %lld selected entr%s",
             static_cast<long long>(paths.size()), paths.size() == 1 ? "y" : "ies");
    if (paths.isEmpty()) {
        LOG_WARN("ArchiveTreeView", "no selected entries, drag not started");
        return;
    }

    // Archive entries are not real files, so only the receiver can provide
    // usable file URLs.  It stages the selected entries and starts the QDrag;
    // this view stays independent of the archive backend and the UI shell.
    // 压缩包条目不是真实文件，只有接收方能提供可用的文件 URL。
    // 接收方暂存选中条目并启动 QDrag；本视图不依赖压缩包后端和 UI 外壳。
    emit dragOutRequested(paths);
}
