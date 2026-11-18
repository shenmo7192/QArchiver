#ifndef APPSETTINGS_H
#define APPSETTINGS_H

// QArchiver / 小Q解压
// Persistent application preferences backed by QSettings.
// 基于 QSettings 的全局应用偏好设置。

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <atomic>
#include <memory>

#include "ArchiveConstants.h"

class QSettings;

// Singleton wrapping QSettings for global preferences (design doc 4.6).
// 使用 QSettings 封装全局偏好设置的单例（设计文档 4.6）。
class AppSettings : public QObject {
    Q_OBJECT
public:
    static AppSettings &instance();
    ~AppSettings() override;

    // Getters / 读取接口
    ArchiveFormat defaultCompressFormat() const;
    int defaultCompressionLevel() const;
    OverwritePolicy overwritePolicy() const;
    bool preservePaths() const;
    bool rememberPasswords() const;
    bool openDestAfterFinish() const;
    qint64 maxPreviewSize() const;
    bool followSymlinks() const;
    QStringList recentFiles() const;
    QString theme() const;
    QString contentViewMode() const;   // "tree" or "list"
    QString toolButtonStyle() const;  // textUnderIcon/textBesideIcon/textOnly/iconOnly
    bool showRecentFiles() const;      // Show the recent-files sidebar
    // Read archives with the libarchive backend instead of the command line.
    // Cached in an atomic so worker threads (PreviewManager) can read it too.
    // 使用 libarchive 后端而不是命令行读取压缩包；缓存在原子变量中，
    // 以便工作线程（PreviewManager）也能安全读取。
    bool useLibArchiveBackend() const;

    // Setters (persist immediately) / 写入接口（立即持久化）
    void setDefaultCompressFormat(ArchiveFormat fmt);
    void setDefaultCompressionLevel(int level);
    void setOverwritePolicy(OverwritePolicy policy);
    void setPreservePaths(bool preserve);
    void setRememberPasswords(bool remember);
    void setOpenDestAfterFinish(bool open);
    void setMaxPreviewSize(qint64 size);
    void setFollowSymlinks(bool follow);
    void setRecentFiles(const QStringList &files);
    void setTheme(const QString &theme);
    void setContentViewMode(const QString &mode);
    void setToolButtonStyle(const QString &style);
    void setShowRecentFiles(bool show);
    void setUseLibArchiveBackend(bool use);

private:
    AppSettings();
    Q_DISABLE_COPY(AppSettings)

    // Read or write one settings key. / 读取或写入单个设置键。
    QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    void setValue(const QString &key, const QVariant &value);

    std::unique_ptr<QSettings> m_settings;
    std::atomic_bool m_useLibArchive{true};
};

#endif // APPSETTINGS_H
