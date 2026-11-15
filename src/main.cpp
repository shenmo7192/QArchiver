#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QLocale>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QLibraryInfo>

#include "ui/MainWindow.h"
#include "utils/Logger.h"
#include "utils/ThemeManager.h"

#ifndef QARCHIVER_VERSION
#define QARCHIVER_VERSION "0.0.1"
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("QArchiver"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("qarchiver.local"));
    QCoreApplication::setApplicationName(QStringLiteral("QArchiver"));
    QCoreApplication::setApplicationVersion(QStringLiteral(QARCHIVER_VERSION));

    // Log file sink: <cache>/QArchiver/app.log
    const QString logDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    Logger::instance().setLogFile(QDir(logDir).filePath(QStringLiteral("app.log")));
    LOG_INFO("main", "QArchiver starting");

    // Parse command line options (e.g. --lang en / --lang zh_CN).
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QArchiver (小Q解压) - lightweight archive manager"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption langOption(
        QStringLiteral("lang"),
        QStringLiteral("Use <lang>.qm (e.g. --lang en or --lang zh_CN)."),
        QStringLiteral("lang"));
    parser.addOption(langOption);
    parser.process(app);

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString installTranslations =
        QDir(appDir).filePath(QStringLiteral("../share/qarchiver/translations"));

    auto loadTranslator = [&app](const QString &name,
                                 const QString &directory) -> bool {
        if (name.isEmpty() || directory.isEmpty()) {
            return false;
        }
        auto *translator = new QTranslator(&app);
        if (translator->load(name, directory)) {
            if (app.installTranslator(translator)) {
                return true;
            }
        }
        delete translator;
        return false;
    };

    auto loadFromKnownPaths = [&](const QString &name) -> bool {
        return loadTranslator(name, QStringLiteral(":/translations"))
            || loadTranslator(name, QDir(appDir).filePath(QStringLiteral("translations")))
            || loadTranslator(name, QDir(appDir).filePath(QStringLiteral("../translations")))
            || loadTranslator(name, appDir)
            || loadTranslator(name, installTranslations)
            || loadTranslator(name, QLibraryInfo::path(QLibraryInfo::TranslationsPath));
    };

    bool translationLoaded = false;

    // --lang: explicit language name.
    if (parser.isSet(langOption)) {
        const QString lang = parser.value(langOption);
        static const QRegularExpression langNameRe(
            QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]*$"));
        if (langNameRe.match(lang).hasMatch()) {
            translationLoaded = loadFromKnownPaths(lang);
        }
    }

    // Default: follow the system locale.  The resource copy is embedded by
    // qt_add_translations(); the other paths are fallbacks for packaged builds.
    if (!translationLoaded) {
        translationLoaded = loadFromKnownPaths(QLocale::system().name());
    }
    if (!translationLoaded) {
        // Some platforms report BCP-47 names (zh-CN); Qt catalogs use zh_CN.
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        for (const QString &uiLang : uiLanguages) {
            QString candidate = uiLang;
            candidate.replace(QLatin1Char('-'), QLatin1Char('_'));
            if (candidate.isEmpty() || candidate == QLatin1String("C")
                || candidate == QLatin1String("POSIX")) {
                continue;
            }
            if (loadFromKnownPaths(candidate)) {
                translationLoaded = true;
                break;
            }
        }
    }

    // Apply the configured theme before any widget is created.
    ThemeManager::applyTheme(app);

    MainWindow window;
    window.show();

    const int ret = app.exec();
    LOG_INFO("main", "QArchiver exiting with code %d", ret);
    return ret;
}
