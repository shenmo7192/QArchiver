// QArchiver / 小Q解压
// Small helpers shared by the MainWindow implementation files.
// 由 MainWindow 各实现文件共享的小型辅助函数。

#include "ui/MainWindowHelpers.h"

#include <QCoreApplication>
#include <QLocale>

namespace MainWindowHelpers {

QString archiveDialogFilter()
{
    // The "MainWindow" context keeps the existing translations valid after the
    // implementation was split into several files.
    // 沿用 "MainWindow" 上下文，使实现文件拆分后现有翻译继续有效。
    return QCoreApplication::translate(
        "MainWindow",
        "Archives (*.tar *.tar.gz *.tgz *.tar.xz *.txz *.tar.bz2 *.tbz2 "
        "*.zip *.7z *.rar *.r?? *.gz *.bz2 *.xz *.001);;All Files (*)");
}

QString formatFileSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes);
}

} // namespace MainWindowHelpers
