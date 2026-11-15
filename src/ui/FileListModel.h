#ifndef FILELISTMODEL_H
#define FILELISTMODEL_H

#include <QAbstractItemModel>
#include <QHash>
#include <QList>

#include "core/ArchiveEntry.h"

// Hierarchical model for archive entries (design doc 6).
// Builds synthetic parent directories so archives that only contain flat
// "dir/file" records still show a browsable tree.
class FileListModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Columns {
        NameColumn = 0,
        SizeColumn,
        PackedSizeColumn,
        ModifiedColumn,
        EncryptedColumn,
        ColumnCount
    };

    enum Roles {
        PathRole = Qt::UserRole, // full internal path
        IsDirRole,               // bool
        IsParentRole             // bool: synthetic ".." entry in list mode
    };

    explicit FileListModel(QObject *parent = nullptr);
    ~FileListModel() override;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    void setEntries(const QList<ArchiveEntry> &entries);
    bool isFlatMode() const { return m_flatMode; }
    void setFlatMode(bool flat);

    // Directory navigation for list mode.  Paths use normalized archive paths.
    QString currentDirectory() const { return m_currentDir; }
    bool enterDirectory(const QString &path);
    bool goUp();
    bool canGoUp() const { return !m_currentDir.isEmpty(); }

    // Flat/list-mode sorting.  The synthetic ".." row is always kept at row 0.
    void sortFlat(int column, Qt::SortOrder order);

    QStringList allPaths() const;

    const ArchiveEntry *entryAt(const QModelIndex &index) const;
    QModelIndex indexForPath(const QString &path) const;

private:
    struct Node {
        ArchiveEntry entry;
        Node *parent = nullptr;
        QList<Node *> children;
        bool synthetic = false;
        bool isParent = false;
    };

    Node *m_root = nullptr;
    QHash<QString, Node *> m_nodes;
    QList<ArchiveEntry> m_sourceEntries;
    bool m_flatMode = false;
    QString m_currentDir;
    int m_flatSortColumn = NameColumn;
    Qt::SortOrder m_flatSortOrder = Qt::AscendingOrder;

    void clear();
    void rebuild();
    void sortFlatNodes();
    static void deleteNode(Node *node);
    Node *ensureDirectory(Node *parent, const QString &path, const QString &name);
    Node *findNode(const QString &path) const;
    bool directoryExists(const QString &path) const;
    QString displayName(const Node *node) const;
};

#endif // FILELISTMODEL_H
