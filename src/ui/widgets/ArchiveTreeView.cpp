#include "ArchiveTreeView.h"
#include "FileListModel.h"

#include <QMouseEvent>
#include <QDrag>
#include <QMimeData>
#include <QUrl>
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
    if (!(event->buttons() & Qt::LeftButton)) {
        return QTreeView::mouseMoveEvent(event);
    }

    // Only begin a drag when the pointer has moved far enough from a valid
    // entry; otherwise pass the event to the base class for normal handling.
    if ((event->pos() - m_dragStartPos).manhattanLength()
        < QApplication::startDragDistance()) {
        return QTreeView::mouseMoveEvent(event);
    }

    const QModelIndex index = indexAt(m_dragStartPos);
    if (!index.isValid()) {
        return QTreeView::mouseMoveEvent(event);
    }

    startDrag(Qt::CopyAction);
}

void ArchiveTreeView::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);

    const QStringList paths = selectedEntryPaths();
    if (paths.isEmpty()) {
        return;
    }

    emit entriesDroppedOut(paths);

    // Drag-out for archive entries is always a copy operation (design doc
    // 5.8.2). We provide the entry list as text and as a custom mime type;
    // MainWindow may extend this with real local file URLs after temporary
    // extraction.
    auto *mimeData = new QMimeData;
    mimeData->setText(paths.join(QLatin1Char('\n')));
    mimeData->setData(QStringLiteral("application/x-qarchiver-entries"),
                      paths.join(QLatin1Char('\n')).toUtf8());

    QDrag drag(this);
    drag.setMimeData(mimeData);
    drag.exec(Qt::CopyAction, Qt::CopyAction);
}
