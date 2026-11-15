#include "FilterProxyModel.h"
#include "FileListModel.h"

#include <QRegularExpression>

FilterProxyModel::FilterProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setDynamicSortFilter(true);
    setRecursiveFilteringEnabled(true);
    setFilterCaseSensitivity(Qt::CaseSensitive);
}

void FilterProxyModel::sort(int column, Qt::SortOrder order)
{
    // In directory/list mode the source model owns sorting so that the
    // synthetic ".." row can stay at row 0 and never participate in sorting.
    if (auto *fileModel = qobject_cast<FileListModel *>(sourceModel());
        fileModel && fileModel->isFlatMode()) {
        // Clear any sorting state left over from tree mode.  Otherwise a
        // source-model reset (entering a directory) makes the proxy sort the
        // new rows again and moves the synthetic ".." entry away from row 0.
        QSortFilterProxyModel::sort(-1, Qt::AscendingOrder);
        fileModel->sortFlat(column, order);
        return;
    }
    QSortFilterProxyModel::sort(column, order);
}

void FilterProxyModel::setFilterWildcard(const QString &pattern)
{
    if (m_pattern == pattern) {
        return;
    }
    m_pattern = pattern;
    beginFilterChange();
    endFilterChange();
}

void FilterProxyModel::setCaseSensitive(bool enabled)
{
    if (m_caseSensitive == enabled) {
        return;
    }
    m_caseSensitive = enabled;
    setFilterCaseSensitivity(enabled ? Qt::CaseSensitive : Qt::CaseInsensitive);
    beginFilterChange();
    endFilterChange();
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

bool FilterProxyModel::filterAcceptsRow(int sourceRow,
                                        const QModelIndex &sourceParent) const
{
    if (!sourceModel()) {
        return false;
    }

    const QModelIndex index = sourceModel()->index(
        sourceRow, FileListModel::NameColumn, sourceParent);
    if (!index.isValid()) {
        return false;
    }

    // Keep the synthetic ".." navigation row visible while filtering.
    if (index.data(FileListModel::IsParentRole).toBool()) {
        return true;
    }

    const QString path = index.data(FileListModel::PathRole).toString();
    const QString name = index.data(Qt::DisplayRole).toString();

    if (!m_pathPrefix.isEmpty()) {
        if (path != m_pathPrefix
            && !path.startsWith(m_pathPrefix + QLatin1Char('/'))) {
            return false;
        }
    }

    if (m_hideEncrypted) {
        const QModelIndex encrypted = sourceModel()->index(
            sourceRow, FileListModel::EncryptedColumn, sourceParent);
        if (!encrypted.data(Qt::DisplayRole).toString().isEmpty()) {
            return false;
        }
    }

    if (!m_pattern.isEmpty()) {
        // Treat a plain search term as a substring match.  Previously the
        // wildcard expression required an exact match, so typing "file"
        // matched neither "file.txt" nor "dir/file.txt" and the view became
        // empty.
        QString pattern = m_pattern;
        if (!pattern.contains(QLatin1Char('*')) && !pattern.contains(QLatin1Char('?'))) {
            pattern = QLatin1Char('*') + pattern + QLatin1Char('*');
        }
        const QRegularExpression::PatternOptions options = m_caseSensitive
            ? QRegularExpression::NoPatternOption
            : QRegularExpression::CaseInsensitiveOption;
        const QRegularExpression re(
            QRegularExpression::wildcardToRegularExpression(pattern), options);
        if (!re.match(path).hasMatch() && !re.match(name).hasMatch()) {
            return false;
        }
    }

    // Qt's recursive filtering accepts a parent if any descendant matches.
    return true;
}
