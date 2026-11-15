#ifndef FILTERPROXYMODEL_H
#define FILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

// Recursive tree filter for archive entries (design doc 5.7).
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
