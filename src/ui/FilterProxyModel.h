#ifndef FILTERPROXYMODEL_H
#define FILTERPROXYMODEL_H

#include <QSortFilterProxyModel>

// Filter + sort proxy model for large archive listings (design doc 5.7)
class FilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit FilterProxyModel(QObject *parent = nullptr);

    void setFilterWildcard(const QString &pattern);
    void setCaseSensitive(bool enabled);
    void setPathFilter(const QString &prefix);
    void setHideEncrypted(bool hide);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    QString m_pattern;
    bool m_caseSensitive = false;
    QString m_pathPrefix;
    bool m_hideEncrypted = false;
};

#endif // FILTERPROXYMODEL_H
