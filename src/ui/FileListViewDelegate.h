#ifndef FILELISTVIEWDELEGATE_H
#define FILELISTVIEWDELEGATE_H

#include <QStyledItemDelegate>

// Custom item delegate for the file list (design doc 6)
class FileListViewDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FileListViewDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

#endif // FILELISTVIEWDELEGATE_H
