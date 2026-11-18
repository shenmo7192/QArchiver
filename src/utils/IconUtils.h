#ifndef QARCHIVER_ICONUTILS_H
#define QARCHIVER_ICONUTILS_H

// QArchiver / 小Q解压
// Small helper for loading embedded application icons.
// 加载内嵌应用图标的小型辅助函数。

#include <QIcon>
#include <QString>

// Small helper for the application's embedded PNG/ICO icon set.
// 用于加载应用内嵌 PNG/ICO 图标集的小型辅助函数。
inline QIcon qarchiverIcon(const QString &fileName)
{
    return QIcon(QStringLiteral(":/icons/") + fileName);
}

#endif // QARCHIVER_ICONUTILS_H
