#ifndef QARCHIVER_APP_TRANSLATIONS_H
#define QARCHIVER_APP_TRANSLATIONS_H

// QArchiver / 小Q解压
// Translation catalog discovery and installation.
// 翻译目录的查找与装载。

#include <QString>

class QCoreApplication;

namespace Translations {

// Install the best matching Qt translation catalog for the requested or the
// system language.  Returns the language name that was applied (empty when no
// catalog matched, i.e. the built-in English strings are used).
// 为指定语言或系统语言装载最匹配的 Qt 翻译目录。返回实际生效的语言名称
// （为空表示没有匹配的目录，即使用内置英文字符串）。
QString install(QCoreApplication &app, const QString &requestedLanguage = QString());

} // namespace Translations

#endif // QARCHIVER_APP_TRANSLATIONS_H
