#ifndef PREVIEWMANAGER_H
#define PREVIEWMANAGER_H

// QArchiver / 小Q解压
// Asynchronous preview extraction and text/image loading.
// 异步预览解压以及文本/图片加载。

#include <QObject>
#include <QString>

#include "core/ArchiveConstants.h"

// Manages temporary extraction for file preview (design doc 5.6).
// Preview extraction never touches the main backend: it creates a private
// backend inside a worker thread, extracts one entry, then destroys it.
// 管理文件预览的临时解压（设计文档 5.6）。
// 预览解压不触碰主后端：在工作线程内创建私有后端，解压单个条目后销毁。
class PreviewManager : public QObject {
    Q_OBJECT
public:
    static PreviewManager &instance();

    void preview(const QString &archivePath, ArchiveFormat format,
                 const QString &entryPath, qint64 entrySize,
                 const QString &password);

    // Cleanup stale preview directories (called at startup)
    // 清理陈旧预览目录（启动时调用）
    void cleanupStale();

    // Cleanup this process's preview directory (called at exit)
    // 清理本进程的预览目录（退出时调用）
    void cleanupOwn();

private:
    PreviewManager();
    Q_DISABLE_COPY(PreviewManager)

    QString previewRoot() const;
    QString ownPreviewDir() const;
};

#endif // PREVIEWMANAGER_H
