// QArchiver / 小Q解压
// FileListViewDelegate painting and sizing.
// FileListViewDelegate 的绘制与尺寸计算。

#include "ui/models/FileListViewDelegate.h"

#include "ui/models/FileListModel.h"
#include "ui/theme/AeroRowColors.h"
#include "ui/widgets/ArchiveTreeView.h"

#include "ui/theme/ThemeManager.h"

#include <QIcon>
#include <QPixmap>

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionViewItem>

FileListViewDelegate::FileListViewDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize FileListViewDelegate::thumbnailDisplaySize(const QSize &source,
                                                  const QSize &limit)
{
    if (source.isEmpty() || limit.isEmpty()) {
        return {};
    }
    QSize scaled = source.scaled(limit, Qt::KeepAspectRatio);
    // QSize::scaled() rounds independently and can collapse an extremely thin
    // panorama to zero height. Keep both dimensions drawable.
    // QSize::scaled() 会分别取整，极端扁长的全景图可能被算成 0 高；
    // 保证两个维度都至少为 1 像素。
    scaled.setWidth(qMax(1, scaled.width()));
    scaled.setHeight(qMax(1, scaled.height()));
    return scaled;
}

void FileListViewDelegate::initStyleOption(
    QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    // Only the modern flat list needs this: its rows stay 26px high while a
    // thumbnail can be much larger. Tree and grid views keep their existing
    // icon sizing, and classic views do not enable thumbnails here.
    // 只处理现代扁平列表：行高固定为 26px，而缩略图可能更大。树/网格
    // 保持现有图标尺寸，经典视图的缩略图开关也不在这里处理。
    if (!ThemeManager::isModernLayout()
        || !index.data(FileListModel::IsThumbnailRole).toBool()) {
        return;
    }
    const auto *view = qobject_cast<const ArchiveTreeView *>(option->widget);
    if (!view || !view->isFlatMode() || option->rect.height() <= 4) {
        return;
    }

    const int targetHeight = option->rect.height() - 4;
    const QSize limit(96, targetHeight);
    const QIcon source = option->icon;
    if (source.isNull()) {
        return;
    }

    QSize sourceSize;
    for (const QSize &candidate : source.availableSizes()) {
        if (!sourceSize.isValid()
            || candidate.width() * candidate.height()
                > sourceSize.width() * sourceSize.height()) {
            sourceSize = candidate;
        }
    }
    if (!sourceSize.isValid()) {
        sourceSize = QSize(128, 128);
    }

    const QPixmap sourcePixmap = source.pixmap(sourceSize);
    if (sourcePixmap.isNull()) {
        return;
    }
    const QSize target = thumbnailDisplaySize(sourcePixmap.size(), limit);
    if (!target.isValid()) {
        return;
    }

    const QPixmap scaled = sourcePixmap.scaled(
        target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    option->icon = QIcon(scaled);
    option->decorationSize = scaled.size();
}

QSize FileListViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Modern rows keep their existing height. Classic rows must fit their icon
    // so a larger decoration size can never overlap the next row.
    // 现代行高保持不变；经典行高必须容纳图标，避免图标尺寸变化后与下一行重叠。
    const int minimumHeight = ThemeManager::isModernLayout()
        ? 26
        : qMax(24, option.decorationSize.height() + 4);
    if (size.height() < minimumHeight) {
        size.setHeight(minimumHeight);
    }
    return size;
}

void FileListViewDelegate::paint(QPainter *painter,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool dark = ThemeManager::isDarkTheme();
    const bool modern = ThemeManager::isModernLayout();
    const auto *view = qobject_cast<const ArchiveTreeView *>(opt.widget);
    const bool selected = opt.state.testFlag(QStyle::State_Selected);
    const bool active = opt.state.testFlag(QStyle::State_Active);
    const bool hovered = view && view->isRowHovered(index);

    if (selected || hovered) {
        const AeroRowColors colors = modern
            ? (selected ? modernSelectedRowColors(dark, active)
                        : modernHoverRowColors(dark))
            : (selected ? aeroSelectedRowColors(dark, active)
                        : aeroHoverRowColors(dark));

        QRect paintRect = opt.rect;
        // Extend the last column highlight to the right edge of the viewport,
        // matching Explorer's full-width selection. The branch/indentation
        // area is handled by ArchiveTreeView::drawBranches().
        // 把最后一列的高亮扩展到视口右边缘，匹配资源管理器的整行选中效果。
        // 分支/缩进区域由 ArchiveTreeView::drawBranches() 处理。
        if (view && index.model()
            && index.column() == index.model()->columnCount(index.parent()) - 1) {
            paintRect.setRight(view->viewport()->width() - 1);
        }

        painter->save();
        painter->fillRect(paintRect, aeroRowGradient(opt.rect, colors));
        if (!modern) {
            painter->setPen(colors.border);
            painter->drawLine(paintRect.left(), paintRect.bottom(),
                              paintRect.right(), paintRect.bottom());
        }
        painter->restore();
    }

    // Let the base delegate draw the icon and text without its own selection
    // or hover background (the Aero row above already provides it).
    // 让基类 delegate 绘制图标与文本，但不再绘制它自己的选中/悬停背景
    // （上面的 Aero 行背景已经完成）。
    QStyleOptionViewItem textOpt(opt);
    textOpt.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver
                       | QStyle::State_HasFocus);
    textOpt.backgroundBrush = Qt::NoBrush;
    if (selected) {
        textOpt.palette.setColor(QPalette::Text,
                                 dark ? QColor(QStringLiteral("#ffffff"))
                                      : QColor(QStringLiteral("#1a1a1a")));
    }
    QStyledItemDelegate::paint(painter, textOpt, index);

    // Windows 7 draws a dotted focus rectangle around the current item.
    // Windows 7 会在当前项周围绘制虚线焦点框。
    if (opt.state.testFlag(QStyle::State_HasFocus)
        && !ThemeManager::isModernLayout()) {
        QPen focusPen(dark ? QColor(QStringLiteral("#d0e8ff"))
                           : QColor(QStringLiteral("#1a1a1a")));
        focusPen.setStyle(Qt::DotLine);
        painter->save();
        painter->setPen(focusPen);
        painter->drawRect(opt.rect.adjusted(1, 1, -2, -2));
        painter->restore();
    }
}
