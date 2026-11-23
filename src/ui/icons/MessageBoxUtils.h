#ifndef MESSAGEBOXUTILS_H
#define MESSAGEBOXUTILS_H

// QArchiver / 小Q解压
// Warning dialogs with the embedded application warning icon.
// 使用内嵌警告图标的消息对话框。

#include <QMessageBox>
#include <QString>
#include <QWidget>

#include "ui/icons/IconUtils.h"
#include "utils/Logger.h"

namespace MessageBoxUtils {

inline void showWarning(QWidget *parent, const QString &title,
                        const QString &text,
                        const QString &informativeText = QString())
{
    QMessageBox box(parent);
    box.setWindowTitle(title);
    box.setIconPixmap(Icons::aero(QStringLiteral("warning.ico")).pixmap(32, 32));
    box.setText(text);
    if (!informativeText.isEmpty()) {
        box.setInformativeText(informativeText);
    }
    box.exec();
}

inline bool showOpenFailure(QWidget *parent, const QString &path)
{
    LOG_WARN("MessageBoxUtils", "Could not open local path: %s",
             path.toUtf8().constData());
    showWarning(parent, QObject::tr("Open Failed"),
                QObject::tr("Cannot open: no application found for this file"),
                QObject::tr("Path: %1\n\n"
                            "The file may be missing, or there may be no "
                            "associated application, insufficient permissions, "
                            "or no available desktop session.")
                    .arg(path));
    return false;
}

} // namespace MessageBoxUtils

#endif // MESSAGEBOXUTILS_H
