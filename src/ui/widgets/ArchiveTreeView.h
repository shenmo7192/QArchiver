#ifndef ARCHIVETREEVIEW_H
#define ARCHIVETREEVIEW_H

#include <QTreeView>
#include <QPoint>

// Tree view for displaying archive contents (design doc 6 - widgets)
class ArchiveTreeView : public QTreeView {
    Q_OBJECT
public:
    explicit ArchiveTreeView(QWidget *parent = nullptr);

    // Switch between hierarchical tree and flat list presentation.
    void setFlatMode(bool flat);

    // Selected archive entry paths (full internal paths, e.g. "dir/file.txt")
    QStringList selectedEntryPaths() const;

signals:
    void entryDoubleClicked(const QString &path);
    // Emitted when the user starts dragging entries out of the view.
    // The receiver is responsible for staging real files and starting the
    // actual QDrag; the view does not know about ArchiveManager at all.
    void dragOutRequested(const QStringList &paths);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void startDrag(Qt::DropActions supportedActions) override;

private:
    QPoint m_dragStartPos;
    bool m_dragActive = false;   // inside startDrag(): ignore re-entrant moves
    bool m_flatMode = false;
};

#endif // ARCHIVETREEVIEW_H
