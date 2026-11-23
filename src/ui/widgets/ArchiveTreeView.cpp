// QArchiver / 小Q解压
// ArchiveTreeView sizing, expansion and selection helpers.
// ArchiveTreeView 的尺寸、展开与选择辅助逻辑。

#include "ui/icons/IconUtils.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/ArchiveTreeView.h"
#include "ui/theme/AeroRowColors.h"
#include "ui/models/FileListModel.h"
#include "core/AppSettings.h"
#include "utils/Logger.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QItemSelectionModel>
#include <QPainter>
#include <QPixmap>
#include <QShowEvent>
#include <QStringList>

// A transparent image layer stacked directly below the tree viewport. The
// viewport itself stays transparent, so the image is visible behind the rows
// and empty space but is clipped to the viewport rectangle. Scroll bars and the
// header are outside that rectangle and therefore never covered by the image.
// 直接堆叠在树 viewport 下方的透明图片层。viewport 本身保持透明，因此图片会
// 显示在行内容和空白区域之后，但被裁剪到 viewport 矩形内。滚动条和表头不在
// 该矩形内，所以不会被图片覆盖。
class ArchiveBackgroundOverlay : public QWidget {
public:
    explicit ArchiveBackgroundOverlay(QWidget *parent)
        : QWidget(parent)
    {
        setObjectName(QStringLiteral("ArchiveBackgroundOverlay"));
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAutoFillBackground(false);
        setFocusPolicy(Qt::NoFocus);
    }

    void setBackground(const BackgroundImageOptions &options,
                       const QPixmap &pixmap)
    {
        m_options = options;
        m_pixmap = pixmap;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        BackgroundImage::paint(&painter, rect(), m_options, m_pixmap);
    }

private:
    BackgroundImageOptions m_options;
    QPixmap m_pixmap;
};

ArchiveTreeView::ArchiveTreeView(QWidget *parent)
    : QTreeView(parent)
{
    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setDragEnabled(true);
    setDragDropMode(QAbstractItemView::DragOnly);
    setAlternatingRowColors(false);
    setMouseTracking(true);
    setSortingEnabled(true);
    setExpandsOnDoubleClick(false);

    m_backgroundOverlay = new ArchiveBackgroundOverlay(this);
    m_backgroundOverlay->stackUnder(viewport());
    m_backgroundOverlay->setGeometry(viewport()->geometry());
    viewport()->installEventFilter(this);

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

void ArchiveTreeView::setBackgroundImage(const BackgroundImageOptions &options)
{
    m_backgroundOptions = options;
    m_backgroundPixmap = options.isValid()
        ? BackgroundImage::loadPixmap(options.path)
        : QPixmap();

    if (options.isValid() && m_backgroundPixmap.isNull()) {
        LOG_WARN_S("ArchiveTreeView",
                   QStringLiteral("Failed to load background image: %1")
                       .arg(options.path));
    }

    if (m_backgroundOverlay) {
        m_backgroundOverlay->setBackground(m_backgroundOptions,
                                           m_backgroundPixmap);
    }
    updateBackgroundOverlayGeometry();
}

void ArchiveTreeView::reloadBackgroundFromSettings()
{
    AppSettings &settings = AppSettings::instance();

    BackgroundImageOptions options;
    options.path = settings.backgroundImagePath();
    options.alignment = BackgroundImage::alignmentFromString(
        settings.backgroundImageAlignment());
    options.size = settings.backgroundImageSize();
    options.opacity = settings.backgroundImageOpacity();
    setBackgroundImage(options);
}

void ArchiveTreeView::updateBackgroundOverlayGeometry()
{
    if (!m_backgroundOverlay) {
        return;
    }
    // viewport() excludes the header, frame and both scroll bars. Keeping the
    // image layer exactly on that rectangle is the whole point of this class.
    // viewport() 已排除表头、边框和两个滚动条；图片层精确覆盖该矩形。
    m_backgroundOverlay->setGeometry(viewport()->geometry());
    m_backgroundOverlay->stackUnder(viewport());
}

bool ArchiveTreeView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == viewport()
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateBackgroundOverlayGeometry();
    }
    return QTreeView::eventFilter(watched, event);
}

void ArchiveTreeView::showEvent(QShowEvent *event)
{
    QTreeView::showEvent(event);
    updateBackgroundOverlayGeometry();
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

bool ArchiveTreeView::isRowHovered(const QModelIndex &index) const
{
    return m_hoveredIndex.isValid() && index.isValid()
        && index.row() == m_hoveredIndex.row()
        && index.parent() == m_hoveredIndex.parent();
}

void ArchiveTreeView::drawBranches(QPainter *painter, const QRect &rect,
                                   const QModelIndex &index) const
{
    // Let QTreeView draw its branch indicators first, then repaint the branch
    // background for selected/hovered rows. The base implementation paints a
    // solid background over the whole branch area, so doing it afterwards is
    // the only way to get a continuous Explorer-style row highlight.
    // 先让 QTreeView 绘制分支指示器，再重绘选中/悬停行的分支背景。
    // 基类会覆盖整个分支区域，因此必须在其后绘制才能得到连续的整行高亮。
    QTreeView::drawBranches(painter, rect, index);

    const bool selected = selectionModel() && selectionModel()->isSelected(index);
    const bool hovered = isRowHovered(index);
    if (!selected && !hovered) {
        return;
    }

    const bool dark = ThemeManager::isDarkTheme();
    const bool modern = ThemeManager::isModernLayout();
    const AeroRowColors colors = modern
        ? (selected ? modernSelectedRowColors(dark, isActiveWindow())
                    : modernHoverRowColors(dark))
        : (selected ? aeroSelectedRowColors(dark, isActiveWindow())
                    : aeroHoverRowColors(dark));

    painter->save();
    painter->fillRect(rect, aeroRowGradient(rect, colors));
    if (!modern) {
        painter->setPen(colors.border);
        painter->drawLine(rect.left(), rect.bottom(), rect.right(), rect.bottom());
    }

    if (model() && model()->hasChildren(index)) {
        // Themed glyphs: the modern set uses "-light" variants on dark
        // backgrounds, the classic set uses "-dark" ones.
        // 主题字形：现代图标集在深色背景使用 "-light" 变体，
        // 经典图标集使用 "-dark" 变体。
        const QPixmap pixmap = Icons::themedPixmap(
            isExpanded(index) ? QStringLiteral("branch-open")
                              : QStringLiteral("branch-closed"));
        const QRect arrowRect(rect.right() - 16, rect.top(), 16, rect.height());
        painter->drawPixmap(arrowRect.center()
                                - QPoint(pixmap.width() / 2, pixmap.height() / 2),
                            pixmap);
    }
    painter->restore();
}

void ArchiveTreeView::leaveEvent(QEvent *event)
{
    if (m_hoveredIndex.isValid()) {
        m_hoveredIndex = QPersistentModelIndex();
        viewport()->update();
    }
    QTreeView::leaveEvent(event);
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
    // Track the row under the mouse so the delegate can paint a full-row
    // Windows 7 style hover highlight.
    // 跟踪鼠标所在行，使 delegate 能绘制 Windows 7 风格的整行悬停高亮。
    const QModelIndex hoverIndex = indexAt(event->pos());
    if (hoverIndex != m_hoveredIndex) {
        m_hoveredIndex = hoverIndex;
        viewport()->update();
    }

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

void ArchiveTreeView::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        emit zoomRequested(event->angleDelta().y());
        event->accept();
        return;
    }
    QTreeView::wheelEvent(event);
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
