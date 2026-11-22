// QArchiver / 小Q解压
// FileListViewDelegate painting and sizing.
// FileListViewDelegate 的绘制与尺寸计算。

#include "FileListViewDelegate.h"

#include "AeroRowColors.h"
#include "widgets/ArchiveTreeView.h"

#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QStyleOptionViewItem>

FileListViewDelegate::FileListViewDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize FileListViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    // Slightly taller rows for readability / 行高略增以提升可读性
    const int minimumHeight = qApp->property("modernUi").toBool() ? 26 : 24;
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

    const bool dark = qApp->property("aeroDark").toBool();
    const bool modern = qApp->property("modernUi").toBool();
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
        && !qApp->property("modernUi").toBool()) {
        QPen focusPen(dark ? QColor(QStringLiteral("#d0e8ff"))
                           : QColor(QStringLiteral("#1a1a1a")));
        focusPen.setStyle(Qt::DotLine);
        painter->save();
        painter->setPen(focusPen);
        painter->drawRect(opt.rect.adjusted(1, 1, -2, -2));
        painter->restore();
    }
}
