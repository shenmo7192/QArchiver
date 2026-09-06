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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("SimpleArchiveManager"));
    QCoreApplication::setApplicationName(QStringLiteral("SimpleArchiveManager"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.0.1"));

    // Log file sink: ~/.cache/SimpleArchiveManager/app.log (design doc 4.7.2)
    const QString logDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    Logger::instance().setLogFile(QDir(logDir).filePath(QStringLiteral("app.log")));
    LOG_INFO("main", "QArchiver starting");

    // Parse command line options (e.g. --lang en / --lang zh_CN).
    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("QArchiver (小Q解压) - lightweight archive manager"));
    parser.addHelpOption();
    const QCommandLineOption langOption(
        QStringLiteral("lang"),
        QStringLiteral("Use <lang>.qm from the application directory "
                       "(e.g. --lang en or --lang zh_CN)."),
        QStringLiteral("lang"));
    parser.addOption(langOption);
    parser.process(app);

    // Load translations. Try the Qt resource first (embedded), then the
    // application directory and the Qt translations path as fallbacks.
    // .qm files are produced from translations/*.ts by lrelease and are
    // named after the locale (e.g. zh_CN.qm).
    auto installTranslator = [&app](const QString &basePath) -> bool {
        if (basePath.isEmpty()) {
            return false;
        }
        auto *translator = new QTranslator(&app);
        if (translator->load(QLocale::system().name(), basePath)) {
            return app.installTranslator(translator);
        }
        delete translator;
        return false;
    };

    bool translationLoaded = false;

    // --lang: prefer an explicit translation next to the executable.
    // If the matching .qm file is not found in the application directory,
    // fall back to the default locale-based lookup below.
    if (parser.isSet(langOption)) {
        const QString lang = parser.value(langOption);
        static const QRegularExpression langNameRe(
            QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]*$"));
        if (langNameRe.match(lang).hasMatch()) {
            auto *translator = new QTranslator(&app);
            if (translator->load(lang, QCoreApplication::applicationDirPath())) {
                app.installTranslator(translator);
                translationLoaded = true;
            } else {
                delete translator;
            }
        }
    }

    // Default logic: follow the system locale.
    if (!translationLoaded) {
        if (!installTranslator(QStringLiteral(":/translations"))) {
            const QString appDir = QCoreApplication::applicationDirPath();
            if (!installTranslator(appDir) && !installTranslator(appDir + QStringLiteral("/translations"))) {
                installTranslator(QLibraryInfo::path(QLibraryInfo::TranslationsPath));
            }
        }
    }

    // Apply the configured theme before any widget is created so the main
    // window and all dialogs start with the correct palette.
    ThemeManager::applyTheme(app);

    MainWindow window;
    window.show();

    const int ret = app.exec();
    LOG_INFO("main", "QArchiver exiting with code %d", ret);
    return ret;
}
