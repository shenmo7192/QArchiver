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
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;

    // Fit a source image into a box while preserving its aspect ratio.
    // 在保持宽高比的前提下，把源图片放入给定尺寸框内。
    static QSize thumbnailDisplaySize(const QSize &source,
                                      const QSize &limit);

protected:
    void initStyleOption(QStyleOptionViewItem *option,
                         const QModelIndex &index) const override;
};

#endif // FILELISTVIEWDELEGATE_H
