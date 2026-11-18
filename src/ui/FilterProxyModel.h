#ifndef FILTERPROXYMODEL_H
#define FILTERPROXYMODEL_H

// QArchiver / 小Q解压
// Recursive filter proxy for archive entry names.
// 用于压缩包条目名称的递归过滤代理模型。

#include <QSortFilterProxyModel>

// Recursive tree filter for archive entries (design doc 5.7).
// 压缩包条目的递归树过滤代理（设计文档 5.7）。
class FilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FilterProxyModel(QObject *parent = nullptr);

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    void setFilterWildcard(const QString &pattern);
    void setCaseSensitive(bool enabled);
    void setPathFilter(const QString &prefix);
    void setHideEncrypted(bool hide);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_pattern;
    bool m_caseSensitive = true;
    QString m_pathPrefix;
    bool m_hideEncrypted = false;
};

#endif // FILTERPROXYMODEL_H
