#include "AppSettings.h"

#include <QCoreApplication>
#include <QSettings>

#include <algorithm>

static constexpr int kMaxRecentFiles = 10;

AppSettings::AppSettings()
{
    const QString organization = QCoreApplication::organizationName().isEmpty()
        ? QStringLiteral("QArchiver")
        : QCoreApplication::organizationName();
    const QString application = QCoreApplication::applicationName().isEmpty()
        ? QStringLiteral("QArchiver")
        : QCoreApplication::applicationName();

    m_settings = std::make_unique<QSettings>(
        QSettings::IniFormat, QSettings::UserScope, organization, application);
}

AppSettings::~AppSettings() = default;

AppSettings &AppSettings::instance()
{
    static AppSettings settings;
    return settings;
}

QVariant AppSettings::value(const QString &key, const QVariant &defaultValue) const
{
    return m_settings->value(key, defaultValue);
}

void AppSettings::setValue(const QString &key, const QVariant &value)
{
    m_settings->setValue(key, value);
    m_settings->sync();
}

static int toInt(ArchiveFormat fmt)
{
    return static_cast<int>(fmt);
}

static ArchiveFormat toFormat(int value)
{
    return static_cast<ArchiveFormat>(value);
}

static int toInt(OverwritePolicy policy)
{
    return static_cast<int>(policy);
}

static OverwritePolicy toPolicy(int value)
{
    return static_cast<OverwritePolicy>(value);
}

ArchiveFormat AppSettings::defaultCompressFormat() const
{
    return toFormat(value(QStringLiteral("compress/defaultFormat"),
                          toInt(ArchiveFormat::Zip)).toInt());
}

int AppSettings::defaultCompressionLevel() const
{
    return value(QStringLiteral("compress/defaultLevel"), 6).toInt();
}

OverwritePolicy AppSettings::overwritePolicy() const
{
    return toPolicy(value(QStringLiteral("extract/overwritePolicy"),
                          toInt(OverwritePolicy::Ask)).toInt());
}

bool AppSettings::preservePaths() const
{
    return value(QStringLiteral("extract/preservePaths"), true).toBool();
}

bool AppSettings::rememberPasswords() const
{
    return value(QStringLiteral("security/rememberPasswords"), false).toBool();
}

bool AppSettings::openDestAfterFinish() const
{
    return value(QStringLiteral("extract/openDestAfterFinish"), false).toBool();
}

qint64 AppSettings::maxPreviewSize() const
{
    return value(QStringLiteral("preview/maxSize"), qint64(52428800)).toLongLong();
}

bool AppSettings::followSymlinks() const
{
    return value(QStringLiteral("compress/followSymlinks"), false).toBool();
}

QStringList AppSettings::recentFiles() const
{
    return value(QStringLiteral("history/recentFiles")).toStringList();
}

QString AppSettings::theme() const
{
    return value(QStringLiteral("ui/theme"), QStringLiteral("system")).toString();
}

QString AppSettings::contentViewMode() const
{
    const QString mode = value(QStringLiteral("ui/contentViewMode"),
                               QStringLiteral("tree")).toString();
    return mode == QStringLiteral("list") ? mode : QStringLiteral("tree");
}

QString AppSettings::toolButtonStyle() const
{
    const QString style = value(QStringLiteral("ui/toolButtonStyle"),
                                QStringLiteral("textUnderIcon")).toString();
    if (style == QStringLiteral("textBesideIcon")
        || style == QStringLiteral("textOnly")
        || style == QStringLiteral("iconOnly")) {
        return style;
    }
    return QStringLiteral("textUnderIcon");
}

void AppSettings::setDefaultCompressFormat(ArchiveFormat fmt)
{
    setValue(QStringLiteral("compress/defaultFormat"), toInt(fmt));
}

void AppSettings::setDefaultCompressionLevel(int level)
{
    setValue(QStringLiteral("compress/defaultLevel"), std::clamp(level, 0, 9));
}

void AppSettings::setOverwritePolicy(OverwritePolicy policy)
{
    setValue(QStringLiteral("extract/overwritePolicy"), toInt(policy));
}

void AppSettings::setPreservePaths(bool preserve)
{
    setValue(QStringLiteral("extract/preservePaths"), preserve);
}

void AppSettings::setRememberPasswords(bool remember)
{
    setValue(QStringLiteral("security/rememberPasswords"), remember);
}

void AppSettings::setOpenDestAfterFinish(bool open)
{
    setValue(QStringLiteral("extract/openDestAfterFinish"), open);
}

void AppSettings::setMaxPreviewSize(qint64 size)
{
    setValue(QStringLiteral("preview/maxSize"), size);
}

void AppSettings::setFollowSymlinks(bool follow)
{
    setValue(QStringLiteral("compress/followSymlinks"), follow);
}

void AppSettings::setRecentFiles(const QStringList &files)
{
    QStringList capped = files;
    while (capped.size() > kMaxRecentFiles) {
        capped.removeLast();
    }
    setValue(QStringLiteral("history/recentFiles"), capped);
}

void AppSettings::setTheme(const QString &theme)
{
    setValue(QStringLiteral("ui/theme"), theme);
}

void AppSettings::setContentViewMode(const QString &mode)
{
    setValue(QStringLiteral("ui/contentViewMode"),
             mode == QStringLiteral("list") ? mode : QStringLiteral("tree"));
}

void AppSettings::setToolButtonStyle(const QString &style)
{
    if (style == QStringLiteral("textBesideIcon")
        || style == QStringLiteral("textOnly")
        || style == QStringLiteral("iconOnly")) {
        setValue(QStringLiteral("ui/toolButtonStyle"), style);
    } else {
        setValue(QStringLiteral("ui/toolButtonStyle"), QStringLiteral("textUnderIcon"));
    }
}
