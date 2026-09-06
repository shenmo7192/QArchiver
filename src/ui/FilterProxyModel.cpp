#include "FilterProxyModel.h"
#include "FileListModel.h"
#include "core/ArchiveEntry.h"

#include <QRegularExpression>

FilterProxyModel::FilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void FilterProxyModel::setFilterWildcard(const QString &pattern)
{
    m_pattern = pattern;
    QSortFilterProxyModel::setFilterWildcard(pattern);
}

void FilterProxyModel::setCaseSensitive(bool enabled)
{
    m_caseSensitive = enabled;
    setFilterCaseSensitivity(enabled ? Qt::CaseSensitive : Qt::CaseInsensitive);
}

void FilterProxyModel::setPathFilter(const QString &prefix)
{
    m_pathPrefix = prefix;
    beginFilterChange();
    endFilterChange();
}

void FilterProxyModel::setHideEncrypted(bool hide)
{
    m_hideEncrypted = hide;
    beginFilterChange();
    endFilterChange();
}

bool FilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex idx = sourceModel()->index(sourceRow, FileListModel::NameColumn, sourceParent);
    if (!idx.isValid()) return false;

    QString path = sourceModel()->data(idx, Qt::UserRole).toString();

    // Path filter: only show entries under a prefix
    if (!m_pathPrefix.isEmpty() && !path.startsWith(m_pathPrefix)) {
        return false;
    }

    // Hide encrypted entries
    if (m_hideEncrypted) {
        QModelIndex encIdx = sourceModel()->index(sourceRow, FileListModel::EncryptedColumn, sourceParent);
        if (!encIdx.data().toString().isEmpty()) {
            return false;
        }
    }

    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}
