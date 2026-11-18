// QArchiver / 小Q解压
// FileListViewDelegate painting and sizing.
// FileListViewDelegate 的绘制与尺寸计算。

#include "FileListViewDelegate.h"

FileListViewDelegate::FileListViewDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize FileListViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Slightly taller rows for readability / 行高略增以提升可读性
    if (size.height() < 24) {
        size.setHeight(24);
    }
    return size;
}
