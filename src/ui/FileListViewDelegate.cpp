#include "FileListViewDelegate.h"

FileListViewDelegate::FileListViewDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void FileListViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                  const QModelIndex &index) const
{
    // Delegate to the default painting; customization can be added later
    QStyledItemDelegate::paint(painter, option, index);
}

QSize FileListViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Slightly taller rows for readability
    if (size.height() < 24) {
        size.setHeight(24);
    }
    return size;
}
