#ifndef QARCHIVER_APP_COMMANDLINE_H
#define QARCHIVER_APP_COMMANDLINE_H

// QArchiver / 小Q解压
// Command line parsing for the application entry point.
// 应用入口的命令行解析。

#include <QString>
#include <QStringList>

class QCoreApplication;

namespace CommandLine {

// Parsed command line options.
// 解析后的命令行选项。
struct Options {
    // Positional arguments (archive to open or extract). / 位置参数（要打开或解压的压缩包）。
    QStringList archives;
    // --lang value, empty when the option was not given. / --lang 的值，未指定时为空。
    QString language;
    // --oneclick: extract instead of opening. / --oneclick：直接解压而不打开。
    bool oneClick = false;
    // --preferences: open the preferences dialog on startup. / --preferences：启动时打开偏好设置。
    bool preferences = false;
    // --benchmark: open the 7-Zip benchmark dialog on startup. / --benchmark：启动时打开 7-Zip 测试对话框。
    bool benchmark = false;
};

// Parse argv/argc of an already constructed application object.  The parser
// also handles --help/--version and may exit the process.
// 解析已构造的应用对象的命令行参数；同时处理 --help/--version，可能直接退出进程。
Options parse(QCoreApplication &app);

} // namespace CommandLine

#endif // QARCHIVER_APP_COMMANDLINE_H
