// QArchiver / 小Q解压
// File kind detection, localized descriptions and icon selection.
// 文件类型识别、本地化描述与图标选择。

#include "ui/icons/FileTypeHelper.h"

#include "common/FormatRegistry.h"
#include "ui/icons/IconUtils.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QHash>
#include <QSet>

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("FileTypeHelper", text);
}

bool isOneOf(const QString &suffix, const QSet<QString> &set)
{
    return set.contains(suffix);
}

// Icon path helpers: the resource roots live in IconUtils. / 图标路径辅助函数：
// 资源根目录由 IconUtils 统一提供。
QString aeroPath(const char *fileName)
{
    return QString::fromLatin1(Icons::aeroRoot()) + QLatin1String(fileName);
}

QString fileTypePath(const char *fileName)
{
    return QString::fromLatin1(Icons::fileTypeRoot()) + QLatin1String(fileName);
}

} // namespace

FileTypeHelper::Kind FileTypeHelper::kindFor(const QString &fileName, bool isDir)
{
    if (isDir) {
        return Kind::Folder;
    }

    const QString suffix = FormatRegistry::kindSuffix(fileName);

    // Archive suffixes (including compound ones) come from FormatRegistry.
    // 压缩包后缀（含复合后缀）由 FormatRegistry 提供。
    if (FormatRegistry::archiveLikeSuffixes().contains(suffix)) {
        return Kind::Archive;
    }

    static const QSet<QString> image = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp"),
        QStringLiteral("svg"), QStringLiteral("ico"), QStringLiteral("tif"),
        QStringLiteral("tiff"), QStringLiteral("heic"), QStringLiteral("avif"),
        QStringLiteral("jxl")
    };
    if (isOneOf(suffix, image)) {
        return Kind::Image;
    }

    static const QSet<QString> text = {
        QStringLiteral("txt"), QStringLiteral("log"), QStringLiteral("md"),
        QStringLiteral("markdown"), QStringLiteral("ini"), QStringLiteral("cfg"),
        QStringLiteral("conf"), QStringLiteral("json"), QStringLiteral("xml"),
        QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("toml"),
        QStringLiteral("rtf")
    };
    if (isOneOf(suffix, text)) {
        return Kind::Text;
    }

    static const QSet<QString> audio = {
        QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"),
        QStringLiteral("aac"), QStringLiteral("ogg"), QStringLiteral("m4a"),
        QStringLiteral("wma"), QStringLiteral("opus"), QStringLiteral("ape")
    };
    if (isOneOf(suffix, audio)) {
        return Kind::Audio;
    }

    static const QSet<QString> video = {
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("wmv"), QStringLiteral("flv"),
        QStringLiteral("webm"), QStringLiteral("m4v"), QStringLiteral("mpg"),
        QStringLiteral("mpeg"), QStringLiteral("ts"), QStringLiteral("rmvb")
    };
    if (isOneOf(suffix, video)) {
        return Kind::Video;
    }

    if (suffix == QStringLiteral("pdf")) {
        return Kind::Pdf;
    }

    static const QSet<QString> document = {
        QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("odt"),
        QStringLiteral("epub"), QStringLiteral("mobi"), QStringLiteral("wps")
    };
    if (isOneOf(suffix, document)) {
        return Kind::Document;
    }

    static const QSet<QString> spreadsheet = {
        QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("ods"),
        QStringLiteral("csv"), QStringLiteral("et")
    };
    if (isOneOf(suffix, spreadsheet)) {
        return Kind::Spreadsheet;
    }

    static const QSet<QString> presentation = {
        QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("odp"),
        QStringLiteral("dps")
    };
    if (isOneOf(suffix, presentation)) {
        return Kind::Presentation;
    }

    static const QSet<QString> code = {
        QStringLiteral("c"), QStringLiteral("h"), QStringLiteral("cc"),
        QStringLiteral("cpp"), QStringLiteral("cxx"), QStringLiteral("hpp"),
        QStringLiteral("py"), QStringLiteral("js"), QStringLiteral("ts"),
        QStringLiteral("java"), QStringLiteral("cs"), QStringLiteral("go"),
        QStringLiteral("rs"), QStringLiteral("sh"), QStringLiteral("bat"),
        QStringLiteral("ps1"), QStringLiteral("html"), QStringLiteral("css"),
        QStringLiteral("php"), QStringLiteral("rb"), QStringLiteral("pl"),
        QStringLiteral("sql"), QStringLiteral("cmake"), QStringLiteral("qml")
    };
    if (isOneOf(suffix, code)) {
        return Kind::Code;
    }

    static const QSet<QString> executable = {
        QStringLiteral("exe"), QStringLiteral("dll"), QStringLiteral("so"),
        QStringLiteral("dylib"), QStringLiteral("bin"), QStringLiteral("com"),
        QStringLiteral("bat"), QStringLiteral("cmd")
    };
    if (isOneOf(suffix, executable)) {
        return Kind::Executable;
    }

    static const QSet<QString> package = {
        QStringLiteral("msi"), QStringLiteral("deb"), QStringLiteral("rpm"),
        QStringLiteral("apk"), QStringLiteral("appimage"), QStringLiteral("snap"),
        QStringLiteral("flatpak"), QStringLiteral("pkg"), QStringLiteral("msix"),
        QStringLiteral("appx"), QStringLiteral("nupkg")
    };
    if (isOneOf(suffix, package)) {
        return Kind::Package;
    }

    static const QSet<QString> font = {
        QStringLiteral("ttf"), QStringLiteral("otf"), QStringLiteral("ttc"),
        QStringLiteral("woff"), QStringLiteral("woff2"), QStringLiteral("eot"),
        QStringLiteral("fon")
    };
    if (isOneOf(suffix, font)) {
        return Kind::Font;
    }

    static const QSet<QString> iso = {
        QStringLiteral("iso"), QStringLiteral("img"), QStringLiteral("dmg"),
        QStringLiteral("vhd"), QStringLiteral("vhdx"), QStringLiteral("wim"),
        QStringLiteral("esd")
    };
    if (isOneOf(suffix, iso)) {
        return Kind::Iso;
    }

    static const QSet<QString> ebook = {
        QStringLiteral("epub"), QStringLiteral("mobi"), QStringLiteral("azw"),
        QStringLiteral("azw3"), QStringLiteral("fb2"), QStringLiteral("cbz"),
        QStringLiteral("cbr")
    };
    if (isOneOf(suffix, ebook)) {
        return Kind::Ebook;
    }

    static const QSet<QString> database = {
        QStringLiteral("db"), QStringLiteral("sqlite"), QStringLiteral("sqlite3"),
        QStringLiteral("mdb"), QStringLiteral("accdb"), QStringLiteral("dbf"),
        QStringLiteral("mdf"), QStringLiteral("ldf"), QStringLiteral("realm")
    };
    if (isOneOf(suffix, database)) {
        return Kind::Database;
    }

    if (suffix == QStringLiteral("torrent")) {
        return Kind::Torrent;
    }

    static const QSet<QString> design = {
        QStringLiteral("psd"), QStringLiteral("ai"), QStringLiteral("fig"),
        QStringLiteral("sketch"), QStringLiteral("xcf"), QStringLiteral("indd"),
        QStringLiteral("afdesign"), QStringLiteral("afphoto"), QStringLiteral("cdr"),
        QStringLiteral("blend")
    };
    if (isOneOf(suffix, design)) {
        return Kind::Design;
    }

    static const QSet<QString> subtitle = {
        QStringLiteral("srt"), QStringLiteral("ass"), QStringLiteral("ssa"),
        QStringLiteral("vtt"), QStringLiteral("sub"), QStringLiteral("idx")
    };
    if (isOneOf(suffix, subtitle)) {
        return Kind::Subtitle;
    }

    static const QSet<QString> threeD = {
        QStringLiteral("obj"), QStringLiteral("stl"), QStringLiteral("3ds"),
        QStringLiteral("fbx"), QStringLiteral("dae"), QStringLiteral("gltf"),
        QStringLiteral("glb"), QStringLiteral("dwg"), QStringLiteral("dxf"),
        QStringLiteral("step"), QStringLiteral("stp"), QStringLiteral("iges"),
        QStringLiteral("igs"), QStringLiteral("ply")
    };
    if (isOneOf(suffix, threeD)) {
        return Kind::ThreeD;
    }

    return Kind::Other;
}

QString FileTypeHelper::description(const QString &fileName, bool isDir)
{
    switch (kindFor(fileName, isDir)) {
    case Kind::Folder:       return tr("Folder");
    case Kind::Archive:      return tr("Archive");
    case Kind::Image:        return tr("Image");
    case Kind::Text:         return tr("Text file");
    case Kind::Audio:        return tr("Audio");
    case Kind::Video:        return tr("Video");
    case Kind::Pdf:          return tr("PDF document");
    case Kind::Document:     return tr("Document");
    case Kind::Spreadsheet:  return tr("Spreadsheet");
    case Kind::Presentation: return tr("Presentation");
    case Kind::Code:         return tr("Source code");
    case Kind::Executable:   return tr("Executable");
    case Kind::Font:         return tr("Font");
    case Kind::Iso:          return tr("Disk image");
    case Kind::Ebook:        return tr("E-book");
    case Kind::Database:     return tr("Database");
    case Kind::Torrent:      return tr("Torrent");
    case Kind::Design:       return tr("Design file");
    case Kind::Subtitle:     return tr("Subtitle");
    case Kind::ThreeD:       return tr("3D / CAD file");
    case Kind::Package:      return tr("Package");
    case Kind::Other:        break;
    }
    return tr("File");
}

QString FileTypeHelper::iconPath(const QString &fileName, bool isDir, bool modern)
{
    if (!modern) {
        return isDir ? aeroPath("folder.png") : aeroPath("file.png");
    }
    if (isDir) {
        return fileTypePath("folder.png");
    }

    switch (kindFor(fileName, false)) {
    case Kind::Folder:       return fileTypePath("folder.png");
    case Kind::Image:        return fileTypePath("file-image.png");
    case Kind::Text:         return fileTypePath("file-text.png");
    case Kind::Audio:        return fileTypePath("file-audio.png");
    case Kind::Video:        return fileTypePath("file-video.png");
    case Kind::Pdf:          return fileTypePath("file-pdf.png");
    case Kind::Document:     return fileTypePath("file-doc.png");
    case Kind::Spreadsheet:  return fileTypePath("file-sheet.png");
    case Kind::Presentation: return fileTypePath("file-ppt.png");
    case Kind::Code:         return fileTypePath("file-code.png");
    case Kind::Executable:   return fileTypePath("file-exe.png");
    case Kind::Font:         return fileTypePath("file-font.png");
    case Kind::Iso:          return fileTypePath("file-iso.png");
    case Kind::Ebook:        return fileTypePath("file-ebook.png");
    case Kind::Database:     return fileTypePath("file-database.png");
    case Kind::Torrent:      return fileTypePath("file-torrent.png");
    case Kind::Design:       return fileTypePath("file-design.png");
    case Kind::Subtitle:     return fileTypePath("file-subtitle.png");
    case Kind::ThreeD:       return fileTypePath("file-3d.png");
    case Kind::Package:      return fileTypePath("file-package.png");
    case Kind::Archive: {
        const QString suffix = FormatRegistry::kindSuffix(fileName);
        if (suffix == QLatin1String("zip")) {
            return fileTypePath("file-zip.png");
        }
        if (suffix == QLatin1String("rar")
            || suffix == QLatin1String("part1.rar")
            || suffix == QLatin1String("partNN.rar")) {
            return fileTypePath("file-rar.png");
        }
        if (suffix == QLatin1String("7z")
            || suffix == QLatin1String("7z.001")) {
            return fileTypePath("file-7z.png");
        }
        return fileTypePath("file-tar.png");
    }
    case Kind::Other:
        break;
    }
    return fileTypePath("file.png");
}

QIcon FileTypeHelper::icon(const QString &fileName, bool isDir, bool modern)
{
    return QIcon(iconPath(fileName, isDir, modern));
}

QString FileTypeHelper::formatLabel(ArchiveFormat format)
{
    switch (format) {
    case ArchiveFormat::Tar:      return QStringLiteral("TAR");
    case ArchiveFormat::TarGz:    return QStringLiteral("TAR.GZ");
    case ArchiveFormat::TarXz:    return QStringLiteral("TAR.XZ");
    case ArchiveFormat::TarBz2:   return QStringLiteral("TAR.BZ2");
    case ArchiveFormat::Zip:      return QStringLiteral("ZIP");
    case ArchiveFormat::SevenZip: return QStringLiteral("7Z");
    case ArchiveFormat::Rar4:
    case ArchiveFormat::Rar5:     return QStringLiteral("RAR");
    case ArchiveFormat::Gzip:     return QStringLiteral("GZ");
    case ArchiveFormat::Bzip2:    return QStringLiteral("BZ2");
    case ArchiveFormat::Xz:       return QStringLiteral("XZ");
    case ArchiveFormat::Unknown:  break;
    }
    return tr("Unknown");
}
