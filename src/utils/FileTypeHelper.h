#ifndef FILETYPEHELPER_H
#define FILETYPEHELPER_H

// QArchiver / 小Q解压
// Human-readable file kinds and modern row icons.
// 可读的文件类型与现代化列表图标。

#include <QIcon>
#include <QString>

#include "core/ArchiveConstants.h"

// Maps a member/file name to a coarse kind, a localized description and the
// icon used by the file list.  The modern icon set lives under :/modern and is
// selected only for the modern layout; the classic layout keeps its original
// Aero icons.
// 将成员/文件名映射为粗略类型、本地化描述及列表图标。
// 现代图标位于 :/modern，仅现代布局使用；经典布局保留原有 Aero 图标。
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
