#ifndef QARCHIVER_UI_MAINWINDOWHELPERS_H
#define QARCHIVER_UI_MAINWINDOWHELPERS_H

// QArchiver / 小Q解压
// Small helpers shared by the MainWindow implementation files.
// 由 MainWindow 各实现文件共享的小型辅助函数。

#include <QString>

namespace MainWindowHelpers {

// File dialog filter listing the archive types QArchiver can open.
// 列出 QArchiver 可打开压缩包类型的文件对话框过滤器。
QString archiveDialogFilter();

// Locale-aware size text, e.g. "1.5 MiB". / 本地化的文件大小文本。
QString formatFileSize(qint64 bytes);

} // namespace MainWindowHelpers

#endif // QARCHIVER_UI_MAINWINDOWHELPERS_H
