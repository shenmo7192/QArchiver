#ifndef ARCHIVETREEVIEW_H
#define ARCHIVETREEVIEW_H

#include <QTreeView>
#include <QPoint>

// Tree view for displaying archive contents (design doc 6 - widgets)
class ArchiveTreeView : public QTreeView {
    Q_OBJECT
public:
    explicit ArchiveTreeView(QWidget *parent = nullptr);

    // Selected archive entry paths (full internal paths, e.g. "dir/file.txt")
    QStringList selectedEntryPaths() const;

signals:
    void entryDoubleClicked(const QString &path);
    // Emitted when the user starts dragging entries out of the view.
    // The receiver is responsible for providing real file URLs if needed.
    void entriesDroppedOut(const QStringList &paths);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    QPoint m_dragStartPos;
};

#endif // ARCHIVETREEVIEW_H
