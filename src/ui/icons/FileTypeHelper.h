#ifndef FILETYPEHELPER_H
#define FILETYPEHELPER_H

// QArchiver / 小Q解压
// Human-readable file kinds and modern row icons.
// 可读的文件类型与现代化列表图标。

#include <QIcon>
#include <QString>

#include "common/ArchiveConstants.h"

// Maps a member/file name to a coarse kind, a localized description and the
// icon used by the file list.  Each layout has its own complete file-type set
// (classic under :/icons/file-types/classic, modern under .../modern).
// 将成员/文件名映射为粗略类型、本地化描述及列表图标。
// 两种布局各自拥有完整的文件类型图标集（经典与现代分别位于不同目录）。
class FileTypeHelper {
public:
    enum class Kind {
        Folder,
        Archive,
        Image,
        Text,
        Audio,
        Video,
        Pdf,
        Document,
        Spreadsheet,
        Presentation,
        Code,
        Executable,
        Font,
        Iso,
        Ebook,
        Database,
        Torrent,
        Design,
        Subtitle,
        ThreeD,
        Package,
        Other
    };

    static Kind kindFor(const QString &fileName, bool isDir);
    static QString description(const QString &fileName, bool isDir);
    static QString iconPath(const QString &fileName, bool isDir, bool modern);
    static QIcon icon(const QString &fileName, bool isDir, bool modern);

    // Short archive-format label for the status bar ("ZIP", "7Z", ...).
    // 状态栏使用的简短压缩格式标签（"ZIP"、"7Z" 等）。
    static QString formatLabel(ArchiveFormat format);
};

#endif // FILETYPEHELPER_H
