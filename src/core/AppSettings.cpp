#include "AppSettings.h"

#include <QSettings>
#include <QStandardPaths>

static constexpr int kMaxRecentFiles = 10;

AppSettings::AppSettings()
{
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));
}

AppSettings &AppSettings::instance()
{
    static AppSettings s;
    return s;
}

static QSettings settingsFile()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("SimpleArchiveManager"), QStringLiteral("settings"));
}

static int toInt(ArchiveFormat fmt)
{
    return static_cast<int>(fmt);
}
static ArchiveFormat toFormat(int v)
{
    return static_cast<ArchiveFormat>(v);
}
static int toInt(OverwritePolicy p)
{
    return static_cast<int>(p);
}
static OverwritePolicy toPolicy(int v)
{
    return static_cast<OverwritePolicy>(v);
}

ArchiveFormat AppSettings::defaultCompressFormat() const
{
    return toFormat(settingsFile().value(QStringLiteral("compress/defaultFormat"),
        toInt(ArchiveFormat::Zip)).toInt());
}

int AppSettings::defaultCompressionLevel() const
{
    return settingsFile().value(QStringLiteral("compress/defaultLevel"), 6).toInt();
}

OverwritePolicy AppSettings::overwritePolicy() const
{
    return toPolicy(settingsFile().value(QStringLiteral("extract/overwritePolicy"),
        toInt(OverwritePolicy::Ask)).toInt());
}

bool AppSettings::preservePaths() const
{
    return settingsFile().value(QStringLiteral("extract/preservePaths"), true).toBool();
}

bool AppSettings::rememberPasswords() const
{
    return settingsFile().value(QStringLiteral("security/rememberPasswords"), false).toBool();
}

bool AppSettings::openDestAfterFinish() const
{
    return settingsFile().value(QStringLiteral("extract/openDestAfterFinish"), false).toBool();
}

qint64 AppSettings::maxPreviewSize() const
{
    return settingsFile().value(QStringLiteral("preview/maxSize"), qint64(52428800)).toLongLong();
}

bool AppSettings::followSymlinks() const
{
    return settingsFile().value(QStringLiteral("compress/followSymlinks"), false).toBool();
}

QStringList AppSettings::recentFiles() const
{
    return settingsFile().value(QStringLiteral("history/recentFiles")).toStringList();
}

QStringList AppSettings::bookmarks() const
{
    return settingsFile().value(QStringLiteral("history/bookmarks")).toStringList();
}

QString AppSettings::theme() const
{
    return settingsFile().value(QStringLiteral("ui/theme"), QStringLiteral("system")).toString();
}

void AppSettings::setDefaultCompressFormat(ArchiveFormat fmt)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("compress/defaultFormat"), toInt(fmt));
}
void AppSettings::setDefaultCompressionLevel(int level)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("compress/defaultLevel"), level);
}
void AppSettings::setOverwritePolicy(OverwritePolicy policy)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("extract/overwritePolicy"), toInt(policy));
}
void AppSettings::setPreservePaths(bool preserve)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("extract/preservePaths"), preserve);
}
void AppSettings::setRememberPasswords(bool remember)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("security/rememberPasswords"), remember);
}
void AppSettings::setOpenDestAfterFinish(bool open)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("extract/openDestAfterFinish"), open);
}
void AppSettings::setMaxPreviewSize(qint64 size)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("preview/maxSize"), size);
}
void AppSettings::setFollowSymlinks(bool follow)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("compress/followSymlinks"), follow);
}
void AppSettings::setRecentFiles(const QStringList &files)
{
    // Cap at kMaxRecentFiles
    QStringList capped = files;
    while (capped.size() > kMaxRecentFiles) {
        capped.removeLast();
    }
    auto s = settingsFile(); s.setValue(QStringLiteral("history/recentFiles"), capped);
}
void AppSettings::setBookmarks(const QStringList &bms)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("history/bookmarks"), bms);
}
void AppSettings::setTheme(const QString &theme)
{
    auto s = settingsFile(); s.setValue(QStringLiteral("ui/theme"), theme);
}
