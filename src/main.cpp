// QArchiver / 小Q解压
// Application entry point: logging, translations, theme and main window.
// 应用入口：日志、翻译、主题与主窗口。

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>

#include "app/CommandLine.h"
#include "app/Translations.h"
#include "ui/MainWindow.h"
#include "ui/theme/ThemeManager.h"
#include "utils/Logger.h"

#ifndef QARCHIVER_VERSION
#define QARCHIVER_VERSION "0.0.1"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Let the desktop associate the window with qarchiver.desktop: this is
    // what launchers and docks use to pick the installed application icon.
    // 让桌面把窗口关联到 qarchiver.desktop；启动器/任务栏据此显示安装的应用图标。
    QGuiApplication::setDesktopFileName(QStringLiteral("qarchiver"));

    QCoreApplication::setOrganizationName(QStringLiteral("QArchiver"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qarchiver.local"));
    QCoreApplication::setApplicationName(QStringLiteral("QArchiver"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QARCHIVER_VERSION));

    // Log file sink: <cache>/QArchiver/app.log
    // 日志文件位置：<cache>/QArchiver/app.log
    const QString logDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    Logger::instance().setLogFile(QDir(logDir).filePath(QStringLiteral("app.log")));
    LOG_INFO("main", "QArchiver starting");

    const CommandLine::Options options = CommandLine::parse(app);

    // Install the UI language before any string is created; QLocale defaults
    // follow the applied catalog so sizes and standard dialogs match.
    // 在创建任何字符串前装载界面语言；QLocale 默认值随生效的翻译目录设置，
    // 使文件大小与标准对话框使用同一语言。
    Translations::install(app, options.language);

    // Apply the configured theme before any widget is created.
    // 在创建任何控件前应用配置的主题。
    ThemeManager::applyTheme(app);

    MainWindow window;
    window.show();

    // Compress from the Explorer context menu / a script: no dialog, the
    // archive is created next to the sources and the window closes afterwards.
    // 来自资源管理器右键菜单/脚本的压缩：不弹出对话框，在源文件旁创建压缩包，
    // 完成后自动关闭窗口。
    if (!options.compressFormat.isEmpty()) {
        QTimer::singleShot(0, &window,
                           [&window, sources = options.archives,
                            format = options.compressFormat,
                            output = options.compressOutput]() {
            window.compressFromCommandLine(sources, format, output);
        });
    } else if (options.compressDialog) {
        QTimer::singleShot(0, &window, [&window, sources = options.archives]() {
            window.compressPathsWithDialog(sources);
        });
    } else if (!options.archives.isEmpty()) {
        // Open a file passed by the desktop environment, a file manager or a
        // Windows shell integration.  --oneclick runs the configured one-click
        // extraction (and optionally closes the window afterwards).
        // 打开桌面环境、文件管理器或 Windows 外壳集成传入的文件。
        // --oneclick 会执行配置好的一键解压（并按设置决定是否关闭窗口）。
        const QString archivePath =
            QFileInfo(options.archives.first()).absoluteFilePath();
        if (options.oneClick) {
            QTimer::singleShot(0, &window, [&window, archivePath]() {
                window.extractOneClickPath(archivePath);
            });
        } else {
            QTimer::singleShot(0, &window, [&window, archivePath]() {
                window.openArchivePath(archivePath);
            });
        }
    }

    if (options.preferences) {
        QTimer::singleShot(0, &window, [&window]() { window.openPreferences(); });
    }
    if (options.benchmark) {
        QTimer::singleShot(0, &window, [&window]() { window.openBenchmark(); });
    }

    const int ret = app.exec();
    LOG_INFO("main", "QArchiver exiting with code %d", ret);
    return ret;
}
