// QArchiver / 小Q解压
// Implementation of the persistent application preference store.
// 全局应用偏好存储的实现。

#include "AppSettings.h"

#include <QCoreApplication>
#include <QSettings>

#include <algorithm>

static constexpr int kDefaultRecentFilesLimit = 20;
static constexpr int kMaxRecentFilesLimit = 100;

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
    m_useLibArchive.store(
        m_settings->value(QStringLiteral("backends/useLibArchive"), true).toBool());
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

int AppSettings::continuousImageDialogThreshold() const
{
    return std::clamp(
        value(QStringLiteral("preview/continuousImageDialogThreshold"), 200).toInt(),
        0, 9999);
}

bool AppSettings::followSymlinks() const
{
    return value(QStringLiteral("compress/followSymlinks"), false).toBool();
}

QStringList AppSettings::recentFiles() const
{
    return value(QStringLiteral("history/recentFiles")).toStringList();
}

int AppSettings::recentFilesLimit() const
{
    return std::clamp(
        value(QStringLiteral("history/recentFilesLimit"),
              kDefaultRecentFilesLimit).toInt(),
        1, kMaxRecentFilesLimit);
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

bool AppSettings::showRecentFiles() const
{
    return value(QStringLiteral("ui/showRecentFiles"), false).toBool();
}

QString AppSettings::backgroundImagePath() const
{
    return value(QStringLiteral("ui/backgroundImage"), QString()).toString();
}

QString AppSettings::backgroundImageAlignment() const
{
    return value(QStringLiteral("ui/backgroundImageAlignment"),
                 QStringLiteral("bottomRight")).toString();
}

QSize AppSettings::backgroundImageSize() const
{
    const int width = value(QStringLiteral("ui/backgroundImageWidth"), 0).toInt();
    const int height = value(QStringLiteral("ui/backgroundImageHeight"), 0).toInt();
    return QSize(std::max(0, width), std::max(0, height));
}

qreal AppSettings::backgroundImageOpacity() const
{
    const qreal opacity = value(QStringLiteral("ui/backgroundImageOpacity"),
                                0.25).toDouble();
    return std::clamp(opacity, 0.0, 1.0);
}

bool AppSettings::useLibArchiveBackend() const
{
    return m_useLibArchive.load();
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

void AppSettings::setContinuousImageDialogThreshold(int count)
{
    setValue(QStringLiteral("preview/continuousImageDialogThreshold"),
             std::clamp(count, 0, 9999));
}

void AppSettings::setFollowSymlinks(bool follow)
{
    setValue(QStringLiteral("compress/followSymlinks"), follow);
}

void AppSettings::setRecentFiles(const QStringList &files)
{
    QStringList capped = files;
    while (capped.size() > recentFilesLimit()) {
        capped.removeLast();
    }
    setValue(QStringLiteral("history/recentFiles"), capped);
}

void AppSettings::setRecentFilesLimit(int limit)
{
    const int clamped = std::clamp(limit, 1, kMaxRecentFilesLimit);
    setValue(QStringLiteral("history/recentFilesLimit"), clamped);

    // Shrink the stored list immediately; RecentFilesManager::trimToLimit()
    // refreshes the sidebar after the preference dialog is accepted.
    // 立即缩短已保存列表；偏好对话框接受后由
    // RecentFilesManager::trimToLimit() 刷新侧边栏。
    QStringList files = recentFiles();
    if (files.size() > clamped) {
        files = files.mid(0, clamped);
        setValue(QStringLiteral("history/recentFiles"), files);
    }
    emit recentFilesLimitChanged();
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

void AppSettings::setShowRecentFiles(bool show)
{
    setValue(QStringLiteral("ui/showRecentFiles"), show);
}

void AppSettings::setBackgroundImagePath(const QString &path)
{
    setValue(QStringLiteral("ui/backgroundImage"), path);
    emit backgroundImageChanged();
}

void AppSettings::setBackgroundImageAlignment(const QString &alignment)
{
    static const QStringList supported = {
        QStringLiteral("topLeft"), QStringLiteral("top"),
        QStringLiteral("topRight"), QStringLiteral("left"),
        QStringLiteral("center"), QStringLiteral("right"),
        QStringLiteral("bottomLeft"), QStringLiteral("bottom"),
        QStringLiteral("bottomRight")
    };
    const QString valueToStore = supported.contains(alignment)
        ? alignment
        : QStringLiteral("bottomRight");
    setValue(QStringLiteral("ui/backgroundImageAlignment"), valueToStore);
    emit backgroundImageChanged();
}

void AppSettings::setBackgroundImageSize(const QSize &size)
{
    setValue(QStringLiteral("ui/backgroundImageWidth"), std::max(0, size.width()));
    setValue(QStringLiteral("ui/backgroundImageHeight"), std::max(0, size.height()));
    emit backgroundImageChanged();
}

void AppSettings::setBackgroundImageOpacity(qreal opacity)
{
    setValue(QStringLiteral("ui/backgroundImageOpacity"),
             std::clamp(opacity, 0.0, 1.0));
    emit backgroundImageChanged();
}

void AppSettings::setBackgroundImageOptions(const QString &path,
                                            const QString &alignment,
                                            const QSize &size, qreal opacity)
{
    static const QStringList supported = {
        QStringLiteral("topLeft"), QStringLiteral("top"),
        QStringLiteral("topRight"), QStringLiteral("left"),
        QStringLiteral("center"), QStringLiteral("right"),
        QStringLiteral("bottomLeft"), QStringLiteral("bottom"),
        QStringLiteral("bottomRight")
    };
    const QString storedAlignment = supported.contains(alignment)
        ? alignment
        : QStringLiteral("bottomRight");

    m_settings->setValue(QStringLiteral("ui/backgroundImage"), path);
    m_settings->setValue(QStringLiteral("ui/backgroundImageAlignment"),
                         storedAlignment);
    m_settings->setValue(QStringLiteral("ui/backgroundImageWidth"),
                         std::max(0, size.width()));
    m_settings->setValue(QStringLiteral("ui/backgroundImageHeight"),
                         std::max(0, size.height()));
    m_settings->setValue(QStringLiteral("ui/backgroundImageOpacity"),
                         std::clamp(opacity, 0.0, 1.0));
    m_settings->sync();
    emit backgroundImageChanged();
}

void AppSettings::setUseLibArchiveBackend(bool use)
{
    setValue(QStringLiteral("backends/useLibArchive"), use);
    // Keep the cached copy in sync for the worker-thread readers.
    // 同步缓存副本，供工作线程读取。
    m_useLibArchive.store(use);
}
