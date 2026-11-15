#ifndef QARCHIVER_ICONUTILS_H
#define QARCHIVER_ICONUTILS_H

#include <QIcon>
#include <QString>

// Small helper for the application's embedded PNG/ICO icon set.
inline QIcon qarchiverIcon(const QString &fileName)
{
    return QIcon(QStringLiteral(":/icons/") + fileName);
}

#endif // QARCHIVER_ICONUTILS_H
