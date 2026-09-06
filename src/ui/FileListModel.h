#ifndef FILELISTMODEL_H
#define FILELISTMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include "core/ArchiveEntry.h"

// Table model for displaying archive entries (design doc 6 - FileListModel)
class FileListModel : public QAbstractTableModel {
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

    explicit FileListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void setEntries(const QList<ArchiveEntry> &entries);
    const QList<ArchiveEntry> &entries() const { return m_entries; }
    const ArchiveEntry *entryAt(int row) const;

private:
    QList<ArchiveEntry> m_entries;
};

#endif // FILELISTMODEL_H
