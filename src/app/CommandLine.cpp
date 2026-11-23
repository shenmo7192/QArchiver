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

    parser.addPositionalArgument(
        QStringLiteral("archive"),
        QStringLiteral("Archive to open (or to extract with --oneclick)."));

    parser.process(app);

    Options options;
    options.archives = parser.positionalArguments();
    if (parser.isSet(langOption)) {
        options.language = parser.value(langOption);
    }
    options.oneClick = parser.isSet(oneClickOption);
    options.preferences = parser.isSet(preferencesOption);
    options.benchmark = parser.isSet(benchmarkOption);
    return options;
}

} // namespace CommandLine
