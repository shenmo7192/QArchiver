// QArchiver / 小Q解压
// FilterProxyModel matching and parent-chain visibility.
// FilterProxyModel 的匹配与父链可见性处理。

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
    // 目录/列表模式下由源模型负责排序，使合成的 ".." 行始终位于第 0 行，
    // 且不参与排序。
    if (auto *fileModel = qobject_cast<FileListModel *>(sourceModel());
        fileModel && fileModel->isFlatMode()) {
        // Clear any sorting state left over from tree mode.  Otherwise a
        // source-model reset (entering a directory) makes the proxy sort the
        // new rows again and moves the synthetic ".." entry away from row 0.
        // 清除树模式遗留的排序状态。否则源模型重置（进入目录）后，
        // 代理会再次排序新行，把合成的 ".." 条目移离第 0 行。
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
    rebuildPatternRegex();
    invalidateFilter();
}

void FilterProxyModel::rebuildPatternRegex()
{
    m_hasPatternRegex = false;
    m_patternRegex = QRegularExpression();
    if (m_pattern.isEmpty()) {
        return;
    }

    // Treat a plain search term as a substring match.  Previously the
    // wildcard expression required an exact match, so typing "file" matched
    // neither "file.txt" nor "dir/file.txt" and the view became empty.
    // 将普通搜索词视为子串匹配。此前通配符表达式要求精确匹配，
    // 输入 "file" 无法匹配 "file.txt" 或 "dir/file.txt"，导致视图为空。
    QString wildcard = m_pattern;
    if (!wildcard.contains(QLatin1Char('*')) && !wildcard.contains(QLatin1Char('?'))) {
        wildcard = QLatin1Char('*') + wildcard + QLatin1Char('*');
    }
    const QRegularExpression::PatternOptions options = m_caseSensitive
        ? QRegularExpression::NoPatternOption
        : QRegularExpression::CaseInsensitiveOption;
    m_patternRegex = QRegularExpression(
        QRegularExpression::wildcardToRegularExpression(wildcard), options);
    m_hasPatternRegex = m_patternRegex.isValid();
}

void FilterProxyModel::setCaseSensitive(bool enabled)
{
    if (m_caseSensitive == enabled) {
        return;
    }
    m_caseSensitive = enabled;
    setFilterCaseSensitivity(enabled ? Qt::CaseSensitive : Qt::CaseInsensitive);
    rebuildPatternRegex();
    invalidateFilter();
}

void FilterProxyModel::setPathFilter(const QString &prefix)
{
    m_pathPrefix = prefix;
    invalidateFilter();
}

void FilterProxyModel::setHideEncrypted(bool hide)
{
    m_hideEncrypted = hide;
    invalidateFilter();
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
    // 过滤时仍保持合成的 ".." 导航行可见。
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

    if (m_hasPatternRegex
        && !m_patternRegex.match(path).hasMatch()
        && !m_patternRegex.match(name).hasMatch()) {
        return false;
    }

    // Qt's recursive filtering accepts a parent if any descendant matches.
    // Qt 的递归过滤会在任一后代匹配时接受父项。
    return true;
}
