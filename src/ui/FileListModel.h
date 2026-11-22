#ifndef FILELISTMODEL_H
#define FILELISTMODEL_H

// QArchiver / 小Q解压
// Tree model that exposes archive entries to the view.
// 向视图暴露压缩包条目的树模型。

#include <QAbstractItemModel>
#include <QHash>
#include <QList>
#include <QSet>

#include "core/ArchiveEntry.h"

// Hierarchical model for archive entries (design doc 6).
// Builds synthetic parent directories so archives that only contain flat
// "dir/file" records still show a browsable tree.
// 压缩包条目的层次模型（设计文档 6）。
// 为只包含扁平 "dir/file" 记录的压缩包构建合成父目录，
// 使其仍能以可浏览的树形展示。
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
    // 列表模式下的目录导航；路径使用规范化后的压缩包路径。
    QString currentDirectory() const { return m_currentDir; }
    bool enterDirectory(const QString &path);
    bool goUp();
    bool canGoUp() const { return !m_currentDir.isEmpty(); }

    // Flat/list-mode sorting.  The synthetic ".." row is always kept at row 0.
    // 扁平/列表模式排序；合成的 ".." 行始终保留在第 0 行。
    void sortFlat(int column, Qt::SortOrder order);

    QStringList allPaths() const;

    // Flat entry list as delivered by the backend (drag-out needs sizes).
    // 后端提供的扁平条目列表（拖出功能需要文件大小）。
    const QList<ArchiveEntry> &sourceEntries() const { return m_sourceEntries; }

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
    // List-mode directory index: normalized directory -> direct children.
    // 列表模式目录索引：规范化目录 -> 直接子项。
    QHash<QString, QList<ArchiveEntry>> m_childrenByDir;
    QSet<QString> m_directoryPaths;
    QSet<QString> m_implicitDirectoryPaths;
    bool m_flatMode = false;
    QString m_currentDir;
    int m_flatSortColumn = NameColumn;
    Qt::SortOrder m_flatSortOrder = Qt::AscendingOrder;

    void clear();
    void buildDirectoryIndex();
    void rebuild();
    void sortFlatNodes();
    static void deleteNode(Node *node);
    Node *ensureDirectory(Node *parent, const QString &path, const QString &name);
    Node *findNode(const QString &path) const;
    bool directoryExists(const QString &path) const;
    QString displayName(const Node *node) const;
};

#endif // FILELISTMODEL_H
