#ifndef RECENTFILESMANAGER_H
#define RECENTFILESMANAGER_H

// QArchiver / 小Q解压
// Persistent most-recently-used archive list.
// 持久化的最近打开压缩包列表。

#include <QObject>
#include <QStringList>

// Manages the recently-opened archives list (design doc 5.9.1).
// 管理最近打开的压缩包列表（设计文档 5.9.1）。
class RecentFilesManager : public QObject {
    Q_OBJECT
public:
    static RecentFilesManager &instance();

    void add(const QString &filePath);
    void remove(const QString &filePath);
    QStringList list() const;
    void clear();
    // Remove entries that no longer exist on disk / 移除磁盘上已不存在的条目
    QStringList validatedList() const;
    // Apply the configured history limit immediately and notify the sidebar.
    // 立即应用配置的历史记录上限并通知侧边栏。
    void trimToLimit();

signals:
    void changed();

private:
    RecentFilesManager();
    Q_DISABLE_COPY(RecentFilesManager)
};

#endif // RECENTFILESMANAGER_H
