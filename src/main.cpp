// QArchiver / 小Q解压
// Application entry point: logging, translations, theme and main window.
// 应用入口：日志、翻译、主题与主窗口。

#include <QApplication>
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QLibraryInfo>
#include <QLocale>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>

#include "ui/MainWindow.h"
#include "utils/Logger.h"
#include "utils/ThemeManager.h"

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

    // Parse command line options (e.g. --lang en / --lang zh_CN).
    // 解析命令行选项（例如 --lang en / --lang zh_CN）。
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QArchiver (小Q解压) - lightweight archive manager"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption langOption(
        QStringLiteral("lang"),
        QStringLiteral("Use <lang> (e.g. --lang en or --lang zh_CN)."),
        QStringLiteral("lang"));
    parser.addOption(langOption);

    const QCommandLineOption oneClickOption(
        QStringLiteral("oneclick"),
        QStringLiteral("Extract the given archive with one-click extraction."));
    parser.addOption(oneClickOption);

    const QCommandLineOption preferencesOption(
        QStringLiteral("preferences"),
        QStringLiteral("Open Preferences on startup."));
    parser.addOption(preferencesOption);

    const QCommandLineOption benchmarkOption(
        QStringLiteral("benchmark"),
        QStringLiteral("Open the 7-Zip benchmark dialog on startup."));
    parser.addOption(benchmarkOption);

    parser.addPositionalArgument(
        QStringLiteral("archive"),
        QStringLiteral("Archive to open (or to extract with --oneclick)."));
    parser.process(app);

    const QString qtTranslationsDir =
        QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    auto loadTranslator = [&app, &qtTranslationsDir](const QString &name) -> bool {
        if (name.isEmpty()) {
            return false;
        }
        // The catalogs are embedded by translations/CMakeLists.txt under the
        // fixed resource prefix "/translations" on every supported Qt version.
        // 翻译目录由 translations/CMakeLists.txt 以固定的资源前缀
        // "/translations" 嵌入，在所有支持的 Qt 版本上路径一致。
        auto *translator = new QTranslator(&app);
        if (!translator->load(name, QStringLiteral(":/translations"))) {
            delete translator;
            return false;
        }
        if (!app.installTranslator(translator)) {
            delete translator;
            return false;
        }

        // Qt's own catalogs localize standard dialog buttons.  They are
        // optional: distributions may not install qt6-translations-l10n.
        // Qt 自身的翻译目录用于本地化标准对话框按钮。
        // 该目录是可选的：发行版未必安装 qt6-translations-l10n。
        const QString qtCatalog = QStringLiteral("qtbase_") + name;
        auto *qtTranslator = new QTranslator(&app);
        if (qtTranslator->load(qtCatalog, qtTranslationsDir)) {
            app.installTranslator(qtTranslator);
            LOG_INFO("main", "Loaded Qt translation catalog '%s' from %s",
                     qPrintable(qtCatalog), qPrintable(qtTranslationsDir));
        } else {
            delete qtTranslator;
        }

        LOG_INFO("main", "Loaded translation catalog '%s' from :/translations",
                 qPrintable(name));
        return true;
    };

    bool translationLoaded = false;
    QString activeLanguage;

    // --lang: explicit language name. / --lang：显式指定语言名称。
    if (parser.isSet(langOption)) {
        const QString lang = parser.value(langOption);
        static const QRegularExpression langNameRe(
            QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]*$"));
        if (langNameRe.match(lang).hasMatch()) {
            translationLoaded = loadTranslator(lang);
            if (translationLoaded) {
                activeLanguage = lang;
            }
        }
    }

    // Default: follow the system locale.  The catalogs are embedded into the
    // executable by qt_add_translations() under :/translations/<lang>.qm.
    // 默认：跟随系统区域设置。翻译目录由 qt_add_translations() 嵌入可执行文件，
    // 运行时路径为 :/translations/<lang>.qm。
    if (!translationLoaded) {
        const QString systemLanguage = QLocale::system().name();
        translationLoaded = loadTranslator(systemLanguage);
        if (translationLoaded) {
            activeLanguage = systemLanguage;
        }
    }
    if (!translationLoaded) {
        // Some platforms report BCP-47 names (zh-CN); Qt catalogs use zh_CN.
        // 某些平台报告 BCP-47 名称（zh-CN）；Qt 翻译目录使用 zh_CN。
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &uiLang : uiLanguages) {
            QString candidate = uiLang;
            candidate.replace(QLatin1Char('-'), QLatin1Char('_'));
            if (candidate.isEmpty() || candidate == QLatin1String("C")
                || candidate == QLatin1String("POSIX")) {
                continue;
            }
            if (loadTranslator(candidate)) {
                translationLoaded = true;
                activeLanguage = candidate;
                break;
            }
        }
    }

    // Keep locale-dependent formatting (file sizes, standard dialogs) in the
    // same language as the user-visible text.
    // 让与区域相关的格式（文件大小、标准对话框）与用户可见文本使用同一语言。
    if (!activeLanguage.isEmpty()) {
        QLocale::setDefault(QLocale(activeLanguage));
    }

    // Apply the configured theme before any widget is created.
    // 在创建任何控件前应用配置的主题。
    ThemeManager::applyTheme(app);

    MainWindow window;
    window.show();

    // Open a file passed by the desktop environment, a file manager or a
    // Windows shell integration.  --oneclick runs the configured one-click
    // extraction (and optionally closes the window afterwards).
    // 打开桌面环境、文件管理器或 Windows 外壳集成传入的文件。
    // --oneclick 会执行配置好的一键解压（并按设置决定是否关闭窗口）。
    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        const QString archivePath =
            QFileInfo(positional.first()).absoluteFilePath();
        if (parser.isSet(oneClickOption)) {
            QTimer::singleShot(0, &window, [&window, archivePath]() {
                window.extractOneClickPath(archivePath);
            });
        } else {
            QTimer::singleShot(0, &window, [&window, archivePath]() {
                window.openArchivePath(archivePath);
            });
        }
    }

    if (parser.isSet(preferencesOption)) {
        QTimer::singleShot(0, &window, [&window]() { window.openPreferences(); });
    }
    if (parser.isSet(benchmarkOption)) {
        QTimer::singleShot(0, &window, [&window]() { window.openBenchmark(); });
    }

    const int ret = app.exec();
    LOG_INFO("main", "QArchiver exiting with code %d", ret);
    return ret;
}
