// QArchiver / 小Q解压
// Application entry point: logging, translations, theme and main window.
// 应用入口：日志、翻译、主题与主窗口。

#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QLibraryInfo>
#include <QLocale>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>

#include "ui/MainWindow.h"
#include "utils/Logger.h"
#include "utils/ThemeManager.h"

#ifndef QARCHIVER_VERSION
#define QARCHIVER_VERSION "0.0.1"
#endif

int main(int argc, char *argv[])
{
    // Force Qt's own file dialogs so the Aero control theme is consistent on
    // every platform. Linux already uses the widget dialog in most setups.
    // 强制使用 Qt 自带的文件对话框，保证各平台的 Aero 控件主题一致。
    // 大多数 Linux 桌面本来也使用 Qt 控件对话框。
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);

    QApplication app(argc, argv);

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

        // Qt's own catalogs localize standard dialog buttons and the
        // non-native file dialog.  They are optional: distributions may
        // not install qt6-translations-l10n.
        // Qt 自身的翻译目录用于本地化标准对话框按钮和非原生文件对话框。
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

    // --lang: explicit language name. / --lang：显式指定语言名称。
    if (parser.isSet(langOption)) {
        const QString lang = parser.value(langOption);
        static const QRegularExpression langNameRe(
            QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]*$"));
        if (langNameRe.match(lang).hasMatch()) {
            translationLoaded = loadTranslator(lang);
        }
    }

    // Default: follow the system locale.  The catalogs are embedded into the
    // executable by qt_add_translations() under :/translations/<lang>.qm.
    // 默认：跟随系统区域设置。翻译目录由 qt_add_translations() 嵌入可执行文件，
    // 运行时路径为 :/translations/<lang>.qm。
    if (!translationLoaded) {
        translationLoaded = loadTranslator(QLocale::system().name());
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
                break;
            }
        }
    }

    // Apply the configured theme before any widget is created.
    // 在创建任何控件前应用配置的主题。
    ThemeManager::applyTheme(app);

    MainWindow window;
    window.show();

    const int ret = app.exec();
    LOG_INFO("main", "QArchiver exiting with code %d", ret);
    return ret;
}
