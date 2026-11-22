#ifndef PLATFORMPROCESS_H
#define PLATFORMPROCESS_H

// QArchiver / 小Q解压
// Platform helpers for launching console tools from a GUI application.
// 从 GUI 应用启动控制台工具的平台辅助函数。

#include <QProcess>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

// On Windows a GUI process that starts a console application (7z.exe, rar.exe)
// briefly opens a console window unless CREATE_NO_WINDOW is set.  Linux and
// other Unix platforms do not need any special handling.
// 在 Windows 上，GUI 进程启动控制台程序（7z.exe、rar.exe）时若不设置
// CREATE_NO_WINDOW，会短暂弹出控制台窗口。Linux 等 Unix 平台无需处理。
inline void qarchiverHideConsoleWindow(QProcess &process)
{
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#else
    Q_UNUSED(process);
#endif
}

#endif // PLATFORMPROCESS_H
