#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
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
        QStringLiteral("Use <lang> (e.g. --lang en or --lang zh_CN)."),
        QStringLiteral("lang"));
    parser.addOption(langOption);
    parser.process(app);

    const QString appDir = QCoreApplication::applicationDirPath();

    auto loadTranslator = [&app](const QString &name) -> bool {
        if (name.isEmpty()) {
            return false;
        }
        auto *translator = new QTranslator(&app);
        if (translator->load(name, QStringLiteral(":/translations"))) {
            if (app.installTranslator(translator)) {
                return true;
            }
        }
        delete translator;
        return false;
    };

    bool translationLoaded = false;

    // --lang: explicit language name.
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
    if (!translationLoaded) {
        translationLoaded = loadTranslator(QLocale::system().name());
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
            if (loadTranslator(candidate)) {
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
