// QArchiver / 小Q解压
// ArchiveGridView setup, selection and drag-out initiation.
// ArchiveGridView 的初始化、选择与拖出启动。

#include "ui/widgets/ArchiveGridView.h"

#include "ui/models/FileListModel.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QStyleOptionViewItem>
#include <QTextLayout>
#include <QTextLine>
#include <QTextOption>
#include <QTimer>

namespace {
// QListView's icon-mode delegate limits the label to the icon width on some
// styles.  Paint the icon with the style and draw the full basename below it.
// 某些样式下 QListView 图标模式会把文字限制在图标宽度内；这里保留样式绘制
// 图标/选中背景，并在图标下方用完整单元格宽度绘制文件名。
class GridItemDelegate : public QStyledItemDelegate {
public:
    explicit GridItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        if (auto *view = qobject_cast<QListView *>(
                const_cast<QWidget *>(option.widget))) {
            const QSize grid = view->gridSize();
            if (grid.isValid()) {
                return grid;
            }
        }
        return QStyledItemDelegate::sizeHint(option, index);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        // QListView's icon-mode option rect is the icon-sized content rect on
        // some styles.  Use the whole cell for background, icon and wrapped
        // label so folder icons are centred and long names can span lines.
        // 某些样式下 QListView 的 option.rect 只有图标大小；这里改用完整单元格
        // 绘制背景、图标和多行文件名，保证文件夹图标居中且长文件名可换行。
        auto *view = qobject_cast<QAbstractItemView *>(
            const_cast<QWidget *>(opt.widget));
        const QRect cellRect = view ? view->visualRect(index) : opt.rect;
        const QRect layoutRect = cellRect.isValid() ? cellRect : opt.rect;
        const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
        const QSize iconSize = (view && view->iconSize().isValid())
            ? view->iconSize()
            : (opt.decorationSize.isValid() ? opt.decorationSize
                                            : QSize(56, 56));
        const QString text = opt.text;

        opt.rect = layoutRect;
        opt.icon = QIcon();
        opt.text.clear();
        QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

        painter->save();
        const bool selected = opt.state & QStyle::State_Selected;
        const int topPad = 8;
        QRect iconRect(QPoint(0, 0), iconSize);
        iconRect.moveCenter(QPoint(layoutRect.center().x(),
                                   layoutRect.top() + topPad
                                       + iconSize.height() / 2));
        if (!icon.isNull()) {
            // QIcon::pixmap() keeps the source aspect ratio (a 16:9 thumbnail
            // yields e.g. 100x56). Draw it at that size and centre it; drawing
            // it into the square iconRect would stretch the preview.
            // QIcon::pixmap() 会保持原比例（例如 16:9 缩略图返回 100x56），
            // 按它自己的尺寸居中绘制；直接画进方形 iconRect 会拉伸预览。
            const QPixmap pixmap = icon.pixmap(iconRect.size());
            if (!pixmap.isNull()) {
                QRect target(QPoint(0, 0), pixmap.size());
                target.moveCenter(iconRect.center());
                painter->drawPixmap(target, pixmap);
            }
        }

        if (!text.isEmpty()) {
            painter->setPen(opt.palette.color(
                selected ? QPalette::HighlightedText : QPalette::Text));
            const int textTop = iconRect.bottom() + 8;
            const QRect textRect(layoutRect.left() + 4, textTop,
                                 layoutRect.width() - 8,
                                 layoutRect.bottom() - textTop - 4);
            painter->setClipRect(textRect);

            QTextOption textOption;
            textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            textOption.setAlignment(Qt::AlignLeft);
            QTextLayout layout(text, opt.font);
            layout.setTextOption(textOption);

            const QFontMetricsF metrics(opt.font);
            const qreal lineHeight = metrics.lineSpacing();
            layout.beginLayout();
            while (true) {
                QTextLine line = layout.createLine();
                if (!line.isValid()) {
                    break;
                }
                line.setLineWidth(textRect.width());
            }
            layout.endLayout();

            const int maxLines = qMax(1, int(textRect.height() / lineHeight));
            const int lines = qMin(layout.lineCount(), maxLines);
            for (int i = 0; i < lines; ++i) {
                const QTextLine line = layout.lineAt(i);
                const qreal x = (textRect.width() - line.naturalTextWidth()) / 2.0;
                line.draw(painter,
                          QPointF(textRect.left() + x,
                                  textRect.top() + i * lineHeight));
            }
        }
        painter->restore();
    }
};
} // namespace

ArchiveGridView::ArchiveGridView(QWidget *parent)
    : QListView(parent)
{
    setObjectName(QStringLiteral("ArchiveGridView"));
    setViewMode(QListView::IconMode);
    setFlow(QListView::LeftToRight);
    setWrapping(true);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setIconSize(QSize(56, 56));
    setGridSize(QSize(160, 150));
    setSpacing(6);
    setUniformItemSizes(true);
    setWordWrap(true);
    setTextElideMode(Qt::ElideNone);
    setFrameShape(QFrame::NoFrame);
    setDragEnabled(false);
    setAcceptDrops(false);
    setItemDelegate(new GridItemDelegate(this));
    viewport()->setAutoFillBackground(false);
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, &QAbstractItemView::activated, this,
            [this](const QModelIndex &index) {
        if (!index.isValid()) {
            return;
        }
        const QString path = index.data(FileListModel::PathRole).toString();
        if (!path.isEmpty()) {
            emit entryActivated(path);
        }
    });
}

void ArchiveGridView::setModel(QAbstractItemModel *model)
{
    QListView::setModel(model);
    if (!model) {
        return;
    }
    const auto schedule = [this]() {
        QTimer::singleShot(0, this, &ArchiveGridView::updateGridHeight);
    };
    connect(model, &QAbstractItemModel::modelReset, this, schedule);
    connect(model, &QAbstractItemModel::layoutChanged, this, schedule);
    connect(model, &QAbstractItemModel::rowsInserted, this, schedule);
    connect(model, &QAbstractItemModel::rowsRemoved, this, schedule);
    connect(model, &QAbstractItemModel::dataChanged, this, schedule);
    schedule();
}

void ArchiveGridView::updateGridHeight()
{
    if (!model()) {
        return;
    }

    const int width = qMax(120, gridSize().width());
    const int iconHeight = iconSize().height();
    const int textWidth = qMax(40, width - 8);
    const QFontMetricsF metrics(font());
    const qreal lineHeight = qMax<qreal>(1, metrics.lineSpacing());

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    int maxLines = 1;
    for (int row = 0; row < model()->rowCount(); ++row) {
        const QString text = model()->index(row, 0).data(Qt::DisplayRole).toString();
        if (text.isEmpty()) {
            continue;
        }
        QTextLayout layout(text, font());
        layout.setTextOption(option);
        layout.beginLayout();
        int lines = 0;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid()) {
                break;
            }
            line.setLineWidth(textWidth);
            ++lines;
        }
        layout.endLayout();
        maxLines = qMax(maxLines, qMax(1, lines));
    }

    const int textHeight = qCeil(maxLines * lineHeight);
    const int height = qMax(iconHeight + 52,
                            8 + iconHeight + 8 + textHeight + 8);
    if (gridSize().height() != height) {
        setGridSize(QSize(width, height));
    }
}

QStringList ArchiveGridView::selectedEntryPaths() const
{
    QStringList paths;
    if (!selectionModel()) {
        return paths;
    }
    const QModelIndexList rows = selectionModel()->selectedRows();
    for (const QModelIndex &index : rows) {
        if (index.data(FileListModel::IsParentRole).toBool()) {
            continue;
        }
        const QString path = index.data(FileListModel::PathRole).toString();
        if (!path.isEmpty()) {
            paths << path;
        }
    }
    return paths;
}

void ArchiveGridView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragStartPos = event->pos();
    }
    QListView::mousePressEvent(event);
}

void ArchiveGridView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        emit zoomRequested(event->angleDelta().y());
        event->accept();
        return;
    }
    QListView::wheelEvent(event);
}

void ArchiveGridView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragActive || !(event->buttons() & Qt::LeftButton)) {
        QListView::mouseMoveEvent(event);
        return;
    }
    if (!indexAt(event->pos()).isValid()) {
        QListView::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - m_dragStartPos).manhattanLength()
        < QApplication::startDragDistance()) {
        QListView::mouseMoveEvent(event);
        return;
    }

    const QStringList paths = selectedEntryPaths();
    if (paths.isEmpty()) {
        return;
    }

    // The MainWindow stages the selected entries and runs the native QDrag.
    // This call blocks until the drop finishes.
    // MainWindow 负责暂存选中条目并启动原生 QDrag；该调用会阻塞到拖放结束。
    m_dragActive = true;
    emit dragOutRequested(paths);
    m_dragActive = false;
}
