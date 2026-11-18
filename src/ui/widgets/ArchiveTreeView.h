#ifndef ARCHIVETREEVIEW_H
#define ARCHIVETREEVIEW_H

// QArchiver / 小Q解压
// Archive-aware tree view with column and keyboard behavior.
// 带列与键盘行为的压缩包树视图。

#include <QTreeView>
#include <QPoint>

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
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    QPoint m_dragStartPos;
    bool m_dragActive = false;   // inside startDrag(): ignore re-entrant moves / 在 startDrag() 内忽略重入移动
    bool m_flatMode = false;
};

#endif // ARCHIVETREEVIEW_H
