#ifndef FILELISTVIEWDELEGATE_H
#define FILELISTVIEWDELEGATE_H

// QArchiver / 小Q解压
// Custom delegate for archive rows and columns.
// 压缩包列表行与列的自定义委托。

#include <QStyledItemDelegate>

// Custom item delegate for the file list (design doc 6).
// 文件列表的自定义条目委托（设计文档 6）。
class FileListViewDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FileListViewDelegate(QObject *parent = nullptr);

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

#endif // FILELISTVIEWDELEGATE_H
