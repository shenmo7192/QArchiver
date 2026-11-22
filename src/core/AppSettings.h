#ifndef APPSETTINGS_H
#define APPSETTINGS_H

// QArchiver / 小Q解压
// Persistent application preferences backed by QSettings.
// 基于 QSettings 的全局应用偏好设置。

#include <QObject>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <Qt>

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
    // Number of same-directory images above which continuous preview shows a
    // preparation dialog.  0 means any image batch.  Range 0..9999.
    // 同目录图片数量超过该值时，连续看图显示准备对话框。0 表示任意图片批次。
    // 范围 0..9999。
    int continuousImageDialogThreshold() const;
    bool followSymlinks() const;
    QStringList recentFiles() const;
    // Maximum entries kept in the recent-files list. Range 1..100.
    // 最近文件列表保留的最大条目数。范围 1..100。
    int recentFilesLimit() const;
    QString theme() const;
    QString contentViewMode() const;   // "tree" or "list"
    QString toolButtonStyle() const;  // textUnderIcon/textBesideIcon/textOnly/iconOnly
    bool showRecentFiles() const;      // Show the recent-files sidebar

    // Background image shown behind the archive content (design doc 10.5).
    // The image path is empty when no background image is configured.
    // 压缩包内容后方的背景图片（设计文档 10.5）。
    // 未配置背景图片时路径为空。
    QString backgroundImagePath() const;
    QString backgroundImageAlignment() const;  // topLeft/top/.../bottomRight
    QSize backgroundImageSize() const;         // 0 x 0 = natural size
    qreal backgroundImageOpacity() const;      // 0.0 .. 1.0
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
    void setContinuousImageDialogThreshold(int count);
    void setFollowSymlinks(bool follow);
    void setRecentFiles(const QStringList &files);
    void setRecentFilesLimit(int limit);
    void setTheme(const QString &theme);
    void setContentViewMode(const QString &mode);
    void setToolButtonStyle(const QString &style);
    void setShowRecentFiles(bool show);
    void setUseLibArchiveBackend(bool use);
    void setBackgroundImagePath(const QString &path);
    void setBackgroundImageAlignment(const QString &alignment);
    void setBackgroundImageSize(const QSize &size);
    void setBackgroundImageOpacity(qreal opacity);
    // Persist all background image options with one sync and one change signal.
    // 一次性持久化全部背景图片选项，只 sync 并发出一次变更信号。
    void setBackgroundImageOptions(const QString &path, const QString &alignment,
                                   const QSize &size, qreal opacity);

signals:
    // Emitted whenever one of the background image options changes.
    // 任一背景图片选项变化时发出。
    void backgroundImageChanged();
    // Emitted after the recent-files limit was persisted and the stored list
    // was trimmed.  RecentFilesManager refreshes the sidebar in response.
    // 最近文件上限持久化并裁剪列表后发出；RecentFilesManager 据此刷新侧边栏。
    void recentFilesLimitChanged();

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
