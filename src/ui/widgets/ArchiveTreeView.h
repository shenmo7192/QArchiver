#ifndef ARCHIVETREEVIEW_H
#define ARCHIVETREEVIEW_H

// QArchiver / 小Q解压
// Archive-aware tree view with column and keyboard behavior.
// 带列与键盘行为的压缩包树视图。

#include <QTreeView>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QPixmap>

#include "ui/background/BackgroundImage.h"

class QPainter;
class QShowEvent;
class ArchiveBackgroundOverlay;

// Tree view for displaying archive contents (design doc 6 - widgets).
// 显示压缩包内容的树视图（设计文档 6 - 控件）。
class ArchiveTreeView : public QTreeView {
    Q_OBJECT
public:
    explicit ArchiveTreeView(QWidget *parent = nullptr);

    // Switch between hierarchical tree and flat list presentation.
    // 在层次树和扁平列表展示之间切换。
    void setFlatMode(bool flat);

    // Selected archive entry paths (full internal paths, e.g. "dir/file.txt")
    // 选中的压缩包条目路径（完整内部路径，例如 "dir/file.txt"）
    QStringList selectedEntryPaths() const;

    // True when the row under the mouse is the same row as index. Used by the
    // file list delegate to paint a full-row Aero hover highlight.
    // 当鼠标所在行与 index 相同时返回 true。
    // 文件列表 delegate 用它绘制整行 Aero 悬停高亮。
    bool isRowHovered(const QModelIndex &index) const;

    // Set the optional semi-transparent image shown only inside the visible
    // viewport area (excluding header and scroll bars). The image is painted
    // behind the rows and stays fixed while the view scrolls.
    // 设置只在可视区域（不含表头和滚动条）内显示的半透明背景图片。
    // 图片绘制在行内容之后，并在滚动时保持固定。
    void setBackgroundImage(const BackgroundImageOptions &options);

    // Reload the image options from AppSettings. / 从 AppSettings 重新加载图片选项。
    void reloadBackgroundFromSettings();

signals:
    void entryDoubleClicked(const QString &path);
    // Emitted when the user starts dragging entries out of the view.
    // The receiver is responsible for staging real files and starting the
    // actual QDrag; the view does not know about ArchiveManager at all.
    // 用户开始从视图拖出条目时发出。
    // 接收方负责准备真实文件并启动实际 QDrag；
    // 视图完全不感知 ArchiveManager。
    void dragOutRequested(const QStringList &paths);

protected:
    void drawBranches(QPainter *painter, const QRect &rect,
                      const QModelIndex &index) const override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    QPoint m_dragStartPos;
    QPersistentModelIndex m_hoveredIndex;
    bool m_dragActive = false;   // inside startDrag(): ignore re-entrant moves / 在 startDrag() 内忽略重入移动
    bool m_flatMode = false;

    // Image layer placed behind the viewport. Keeping it as a sibling of the
    // viewport (instead of painting in the central widget) clips it exactly to
    // the visible content area and automatically excludes the scroll bars.
    // 放在 viewport 之后的图片层。作为 viewport 的兄弟控件而不是在中央控件上
    // 绘制，能精确裁剪到可视内容区，并自动排除滚动条。
    ArchiveBackgroundOverlay *m_backgroundOverlay = nullptr;
    BackgroundImageOptions m_backgroundOptions;
    QPixmap m_backgroundPixmap;

    void updateBackgroundOverlayGeometry();
};

#endif // ARCHIVETREEVIEW_H
