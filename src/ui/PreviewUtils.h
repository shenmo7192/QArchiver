#ifndef PREVIEWUTILS_H
#define PREVIEWUTILS_H

// QArchiver / 小Q解压
// Helpers for selecting images that belong to the same archive directory.
// 选取压缩包内同一目录图片的辅助函数。

#include <QList>
#include <QString>
#include <QStringList>

#include "core/ArchiveEntry.h"

namespace PreviewUtils {

// True for the image extensions supported by continuous preview.
// 是否为连续看图支持的图片后缀。
bool isSupportedImagePath(const QString &path);

// Directory part of a normalized archive entry path (empty for the root).
// 规范化压缩包条目路径的目录部分（根目录返回空字符串）。
QString parentDirectory(const QString &entryPath);

// Whether continuous preview should show the modal preparation dialog.
// A threshold of 0 means every non-cached batch; fully cached batches never
// show it.  The comparison is strictly greater than the threshold.
// 连续看图是否应显示模态准备对话框。阈值 0 表示所有未完全缓存的批次；
// 已全部缓存时不显示。比较为严格大于阈值。
bool shouldShowPreparationDialog(int imageCount, int threshold, bool allCached);

// All direct, non-symlink image entries in the same archive directory as
// @p entryPath.  Subdirectories are not descended into.  The clicked entry is
// included when it appears in @p entries.
// 返回与 @p entryPath 同目录的直接图片条目，不进入子目录，跳过符号链接。
// 点击的条目若存在于 @p entries 中，也会包含在结果内。
QStringList imageSiblings(const QList<ArchiveEntry> &entries,
                          const QString &entryPath);

} // namespace PreviewUtils

#endif // PREVIEWUTILS_H
