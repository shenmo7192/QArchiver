// QArchiver / 小Q解压
// Translation catalog discovery and installation.
// 翻译目录的查找与装载。

#include "app/Translations.h"

#include <QCoreApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QTranslator>

#include "utils/Logger.h"

namespace {

// Load :/translations/<name>.qm plus Qt's own qtbase_<name>.qm.
// 装载 :/translations/<name>.qm 以及 Qt 自带的 qtbase_<name>.qm。
bool loadTranslator(QCoreApplication &app, const QString &name)
{
    if (name.isEmpty()) {
        return false;
    }

    // The catalogs are embedded by translations/CMakeLists.txt under the fixed
    // resource prefix "/translations" on every supported Qt version.
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

    // Qt's own catalogs localize standard dialog buttons.  They are optional:
    // distributions may not install qt6-translations-l10n.
    // Qt 自身的翻译目录用于本地化标准对话框按钮。
    // 该目录是可选的：发行版未必安装 qt6-translations-l10n。
    const QString qtTranslationsDir =
        QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    const QString qtCatalog = QStringLiteral("qtbase_") + name;
    auto *qtTranslator = new QTranslator(&app);
    if (qtTranslator->load(qtCatalog, qtTranslationsDir)) {
        app.installTranslator(qtTranslator);
        LOG_INFO("Translations", "Loaded Qt translation catalog '%s' from %s",
                 qPrintable(qtCatalog), qPrintable(qtTranslationsDir));
    } else {
        delete qtTranslator;
    }

    LOG_INFO("Translations", "Loaded translation catalog '%s' from :/translations",
             qPrintable(name));
    return true;
}

} // namespace

namespace Translations {

QString install(QCoreApplication &app, const QString &requestedLanguage)
{
    // --lang: explicit language name. / --lang：显式指定语言名称。
    if (!requestedLanguage.isEmpty()) {
        static const QRegularExpression langNameRe(
            QStringLiteral("^[A-Za-z][A-Za-z0-9_.-]*$"));
        if (langNameRe.match(requestedLanguage).hasMatch()
            && loadTranslator(app, requestedLanguage)) {
            QLocale::setDefault(QLocale(requestedLanguage));
            return requestedLanguage;
        }
    }

    // Default: follow the system locale. / 默认：跟随系统区域设置。
    const QString systemLanguage = QLocale::system().name();
    if (loadTranslator(app, systemLanguage)) {
        QLocale::setDefault(QLocale(systemLanguage));
        return systemLanguage;
    }

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
        if (loadTranslator(app, candidate)) {
            // Keep locale-dependent formatting (file sizes, standard dialogs)
            // in the same language as the user-visible text.
            // 让与区域相关的格式（文件大小、标准对话框）与用户可见文本使用同一语言。
            QLocale::setDefault(QLocale(candidate));
            return candidate;
        }
    }

    return QString();
}

} // namespace Translations
