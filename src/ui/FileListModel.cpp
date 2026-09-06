#include "FileListModel.h"

#include <QIcon>
#include <QFileInfo>
#include <QLocale>

FileListModel::FileListModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int FileListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_entries.size();
}

int FileListModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColumnCount;
}

QVariant FileListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const ArchiveEntry &e = m_entries.at(index.row());

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case NameColumn: {
            // Show only the filename portion for readability
            QString name = e.path;
            int slash = name.lastIndexOf('/');
            return slash >= 0 ? name.mid(slash + 1) : name;
        }
        case SizeColumn:
            return QLocale().formattedDataSize(e.size);
        case PackedSizeColumn:
            return QLocale().formattedDataSize(e.packedSize);
        case ModifiedColumn:
            return e.mtime.toString(Qt::ISODate);
        case EncryptedColumn:
            return e.isEncrypted ? tr("Yes") : QString();
        }
    } else if (role == Qt::DecorationRole && index.column() == NameColumn) {
        return e.isDir ? QIcon(QStringLiteral(":/icons/folder.png"))
                       : QIcon(QStringLiteral(":/icons/file.png"));
    } else if (role == Qt::ToolTipRole) {
        return e.path;
    } else if (role == Qt::UserRole) {
        return e.path;  // Full path for selection
    }
    return {};
}

QVariant FileListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal) {
        return {};
    }
    switch (section) {
    case NameColumn:       return tr("Name");
    case SizeColumn:       return tr("Size");
    case PackedSizeColumn: return tr("Packed");
    case ModifiedColumn:   return tr("Modified");
    case EncryptedColumn:  return tr("Encrypted");
    }
    return {};
}

void FileListModel::setEntries(const QList<ArchiveEntry> &entries)
{
    beginResetModel();
    m_entries = entries;
    endResetModel();
}

const ArchiveEntry *FileListModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return nullptr;
    }
    return &m_entries.at(row);
}
