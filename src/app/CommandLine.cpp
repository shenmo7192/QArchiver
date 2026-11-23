// QArchiver / 小Q解压
// Command line parsing for the application entry point.
// 应用入口的命令行解析。

#include "app/CommandLine.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

namespace CommandLine {

Options parse(QCoreApplication &app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QArchiver (小Q解压) - lightweight archive manager"));
    parser.addHelpOption();
    parser.addVersionOption();

    // --lang: explicit UI language (e.g. --lang en / --lang zh_CN).
    // --lang：显式指定界面语言（例如 --lang en / --lang zh_CN）。
    const QCommandLineOption langOption(
        QStringLiteral("lang"),
        QStringLiteral("Use <lang> (e.g. --lang en or --lang zh_CN)."),
        QStringLiteral("lang"));
    parser.addOption(langOption);

    // --oneclick: run the configured one-click extraction. / --oneclick：执行一键解压。
    const QCommandLineOption oneClickOption(
        QStringLiteral("oneclick"),
        QStringLiteral("Extract the given archive with one-click extraction."));
    parser.addOption(oneClickOption);

    // --preferences: open the preferences dialog on startup. / --preferences：启动时打开偏好设置。
    const QCommandLineOption preferencesOption(
        QStringLiteral("preferences"),
        QStringLiteral("Open Preferences on startup."));
    parser.addOption(preferencesOption);

    // --benchmark: open the 7-Zip benchmark dialog on startup. / --benchmark：启动时打开 7-Zip 测试对话框。
    const QCommandLineOption benchmarkOption(
        QStringLiteral("benchmark"),
        QStringLiteral("Open the 7-Zip benchmark dialog on startup."));
    parser.addOption(benchmarkOption);

    // --compress <format>: create an archive from the positional paths, using
    // "<name>.<ext>" next to the sources unless --output is given.  Used by the
    // Windows Explorer context menu ("Add to <name>.zip" / "Compress to").
    // --compress <格式>：用位置参数创建压缩包；默认在源文件旁生成
    // “<名称>.<扩展名>”，也可用 --output 指定目标。Windows 资源管理器右键菜单
    //（“添加到 <名称>.zip”/“压缩到…”）使用该选项。
    const QCommandLineOption compressOption(
        QStringLiteral("compress"),
        QStringLiteral("Create an archive from the given files using <format> "
                       "(zip, 7z, tar, tar.gz, tar.xz, tar.bz2)."),
        QStringLiteral("format"));
    parser.addOption(compressOption);

    const QCommandLineOption outputOption(
        QStringLiteral("output"),
        QStringLiteral("Archive path to create with --compress."),
        QStringLiteral("path"));
    parser.addOption(outputOption);

    // --compress-dialog: open the normal compress dialog for the positional
    // paths (Explorer "Compress with options...").
    // --compress-dialog：为位置参数打开常规压缩对话框（资源管理器“自定义压缩”）。
    const QCommandLineOption compressDialogOption(
        QStringLiteral("compress-dialog"),
        QStringLiteral("Open the compress dialog for the given files."));
    parser.addOption(compressDialogOption);

    // Association helpers used by the Windows installer/uninstaller.
    // Windows 安装器/卸载器使用的文件关联辅助选项。
    const QCommandLineOption registerAssocOption(
        QStringLiteral("register-associations"),
        QStringLiteral("Register archive file associations for the current "
                       "user and exit."));
    parser.addOption(registerAssocOption);
    const QCommandLineOption unregisterAssocOption(
        QStringLiteral("unregister-associations"),
        QStringLiteral("Remove archive file associations for the current "
                       "user and exit."));
    parser.addOption(unregisterAssocOption);

    parser.addPositionalArgument(
        QStringLiteral("archive"),
        QStringLiteral("Archive to open (or to extract with --oneclick), or "
                       "files to compress with --compress."));

    parser.process(app);

    Options options;
    options.archives = parser.positionalArguments();
    if (parser.isSet(langOption)) {
        options.language = parser.value(langOption);
    }
    options.oneClick = parser.isSet(oneClickOption);
    options.preferences = parser.isSet(preferencesOption);
    options.benchmark = parser.isSet(benchmarkOption);
    if (parser.isSet(compressOption)) {
        options.compressFormat = parser.value(compressOption);
    }
    if (parser.isSet(outputOption)) {
        options.compressOutput = parser.value(outputOption);
    }
    options.compressDialog = parser.isSet(compressDialogOption);
    options.registerAssociations = parser.isSet(registerAssocOption);
    options.unregisterAssociations = parser.isSet(unregisterAssocOption);
    return options;
}

} // namespace CommandLine
