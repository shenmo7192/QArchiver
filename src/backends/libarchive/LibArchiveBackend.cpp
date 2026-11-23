// QArchiver / 小Q解压
// LibArchiveBackend listing, extraction and stream handling.
// LibArchiveBackend 的列表、解压与流处理。

#include "backends/libarchive/LibArchiveBackend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <memory>
#include <string>

#include <archive.h>
#include <archive_entry.h>

#ifndef Q_OS_WIN
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "utils/FileUtils.h"
#include "utils/Logger.h"

namespace {

// Read/write granularity handed to libarchive. 256 KiB keeps the syscall count
// low without hurting the progress/cancel latency.
// 交给 libarchive 的读写粒度。256 KiB 可减少系统调用次数，
// 同时不会明显影响进度更新与取消延迟。
constexpr size_t kBlockSize = 256 * 1024;

struct ReaderDeleter {
    void operator()(struct archive *reader) const
    {
        if (reader) {
            archive_read_free(reader);
        }
    }
};
using ReaderPtr = std::unique_ptr<struct archive, ReaderDeleter>;

// libarchive module name, used to pass per-format options such as hdrcharset.
// libarchive 模块名，用于传递 hdrcharset 等按格式区分的选项。
const char *moduleName(ArchiveFormat fmt)
{
    switch (fmt) {
    case ArchiveFormat::Zip:
        return "zip";
    case ArchiveFormat::SevenZip:
        return "7zip";
    case ArchiveFormat::Tar:
    case ArchiveFormat::TarGz:
    case ArchiveFormat::TarXz:
    case ArchiveFormat::TarBz2:
    case ArchiveFormat::TarZst:
        return "tar";
    case ArchiveFormat::Gzip:
    case ArchiveFormat::Bzip2:
    case ArchiveFormat::Xz:
    case ArchiveFormat::Zstd:
    case ArchiveFormat::Rar4:
    case ArchiveFormat::Rar5:
    case ArchiveFormat::Unknown:
        break;
    }
    return nullptr;
}

// libarchive exposes entry names as UTF-8 when the archive carries them; legacy
// archives (CP437/GBK in zip, unrar'ed tar) only have byte strings.
// 压缩包内带编码信息时，libarchive 以 UTF-8 暴露条目名；
// 旧式压缩包（zip 中的 CP437/GBK、tar）只有字节串。
QString entryPathString(struct archive_entry *entry)
{
    if (const char *utf8 = archive_entry_pathname_utf8(entry)) {
        return QString::fromUtf8(utf8);
    }
    if (const char *raw = archive_entry_pathname(entry)) {
        return QString::fromLocal8Bit(raw);
    }
    return QString();
}

QString libarchiveError(struct archive *reader)
{
    if (const char *error = archive_error_string(reader)) {
        return QString::fromUtf8(error);
    }
    return QStringLiteral("unknown libarchive error");
}

ReaderPtr openReader(const QString &filePath, ArchiveFormat fmt,
                     const QString &password, const QString &encodingHint,
                     QString *errorOut)
{
    ReaderPtr reader(archive_read_new());
    if (!reader) {
        *errorOut = QStringLiteral("libarchive: failed to allocate a reader");
        return {};
    }

    archive_read_support_filter_all(reader.get());
    archive_read_support_format_all(reader.get());
    // Pure gz/bz2/xz streams have no header of their own; the raw reader
    // exposes them as a single unnamed entry.
    // 纯 gz/bz2/xz 流没有自身头部；raw 读取器将其作为单个无名条目暴露。
    if (isStreamFamily(fmt)) {
        archive_read_support_format_raw(reader.get());
    }

    if (!encodingHint.isEmpty()) {
        if (const char *module = moduleName(fmt)) {
            // Only the format-specific form is honoured by libarchive.
            // libarchive 仅识别带格式前缀的形式。
            if (archive_read_set_option(reader.get(), module, "hdrcharset",
                                        encodingHint.toUtf8().constData())
                < ARCHIVE_WARN) {
                LOG_INFO("LibArchiveBackend", "Unsupported hdrcharset hint: %s",
                         encodingHint.toUtf8().constData());
            }
        }
    }

    if (!password.isEmpty()) {
        archive_read_add_passphrase(reader.get(), password.toUtf8().constData());
    }

    const QByteArray nativePath = QFile::encodeName(filePath);
    if (archive_read_open_filename(reader.get(), nativePath.constData(), kBlockSize)
        != ARCHIVE_OK) {
        *errorOut = libarchiveError(reader.get());
        return {};
    }
    return reader;
}

bool isUnderSkipped(const QString &path, const QSet<QString> &skipped)
{
    if (skipped.isEmpty()) {
        return false;
    }
    const QString normalized = FileUtils::normalizeEntryPath(path);
    if (skipped.contains(normalized)) {
        return true;
    }
    for (const QString &prefix : skipped) {
        if (normalized.startsWith(prefix + QLatin1Char('/'))) {
            return true;
        }
    }
    return false;
}

#ifdef Q_OS_WIN
#ifndef SYMBOLIC_LINK_FLAG_DIRECTORY
#define SYMBOLIC_LINK_FLAG_DIRECTORY 0x1
#endif
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

bool createSymbolicLink(const QString &target, const QString &linkPath)
{
    QFile::remove(linkPath);
    const QString nativeLink = QDir::toNativeSeparators(linkPath);
    const QString nativeTarget = QDir::toNativeSeparators(target);
    const std::wstring linkW = nativeLink.toStdWString();
    const std::wstring targetW = nativeTarget.toStdWString();
    const DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if (CreateSymbolicLinkW(linkW.c_str(), targetW.c_str(), flags) != 0) {
        return true;
    }
    return CreateSymbolicLinkW(linkW.c_str(), targetW.c_str(),
                               flags | SYMBOLIC_LINK_FLAG_DIRECTORY) != 0;
}
#endif

bool createHardLink(const QString &target, const QString &linkPath)
{
#ifndef Q_OS_WIN
    QFile::remove(linkPath);
    const QByteArray nativeTarget = QFile::encodeName(target);
    const QByteArray nativeLink = QFile::encodeName(linkPath);
    return ::link(nativeTarget.constData(), nativeLink.constData()) == 0;
#else
    QFile::remove(linkPath);
    return CreateHardLinkW(
               reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(linkPath).utf16()),
               reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(target).utf16()),
               nullptr)
        != 0;
#endif
}

} // namespace

LibArchiveBackend::LibArchiveBackend(ArchiveFormat fmt, QObject *parent)
    : ArchiveBackend(parent)
    , m_format(fmt)
{
}

QString LibArchiveBackend::inferStreamName() const
{
    QFileInfo info(m_archivePath);
    QString baseName = info.fileName();
    const QStringList suffixes = {QStringLiteral(".gz"), QStringLiteral(".bz2"),
                                  QStringLiteral(".xz")};
    for (const QString &suffix : suffixes) {
        if (baseName.endsWith(suffix, Qt::CaseInsensitive)) {
            baseName.chop(suffix.size());
            break;
        }
    }
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("output");
    }
    return baseName;
}

void LibArchiveBackend::openArchive(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists()) {
        emit errorOccurred(ArchiveErrorCode::FileNotFound,
                           tr("File not found: %1").arg(filePath));
        return;
    }
    if (!info.isReadable()) {
        emit errorOccurred(ArchiveErrorCode::PermissionDenied,
                           tr("File is not readable: %1").arg(filePath));
        return;
    }

    m_archivePath = filePath;
    m_entries.clear();
    m_isOpen = true;
    emit archiveOpened(filePath);
}

void LibArchiveBackend::reportFailure(const QString &message,
                                      const QString &password,
                                      const QString &context)
{
    const QString lower = message.toLower();

    const bool needsPassword = lower.contains(QStringLiteral("passphrase required"))
        || lower.contains(QStringLiteral("requires a password"))
        || lower.contains(QStringLiteral("password required"));
    const bool wrongPassword = lower.contains(QStringLiteral("incorrect passphrase"))
        || lower.contains(QStringLiteral("wrong password"))
        || lower.contains(QStringLiteral("passphrase is incorrect"));

    if (needsPassword || wrongPassword) {
        if (password.isEmpty()) {
            emit passwordRequested();
            return;
        }
        emit errorOccurred(ArchiveErrorCode::WrongPassword, tr("Wrong password"));
        return;
    }

    // libarchive knows the container but not its crypto (e.g. encrypted 7z).
    // UnsupportedFormat lets the manager retry with the CLI backend.
    // libarchive 能识别容器但无法处理其加密（例如加密 7z）。
    // 返回 UnsupportedFormat 可让管理器改用 CLI 后端重试。
    if (lower.contains(QStringLiteral("encrypted"))) {
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
                           tr("%1: %2").arg(context, message));
        return;
    }
    if (lower.contains(QStringLiteral("cannot be converted from utf-8"))
        || lower.contains(QStringLiteral("iconv"))) {
        emit errorOccurred(ArchiveErrorCode::EncodingError,
                           tr("%1: %2").arg(context, message));
        return;
    }

    LOG_INFO("LibArchiveBackend", "%s failed: %s", context.toUtf8().constData(),
             message.toUtf8().constData());
    emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                       tr("%1: %2").arg(context, message));
}

void LibArchiveBackend::listContents(const ListOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    QString error;
    ReaderPtr reader = openReader(m_archivePath, m_format, options.password,
                                 options.encodingHint, &error);
    if (!reader) {
        reportFailure(error, options.password, tr("Cannot open archive"));
        return;
    }

    QList<ArchiveEntry> entries;
    struct archive_entry *entry = nullptr;
    int result = ARCHIVE_OK;

    while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        QString path = FileUtils::normalizeEntryPath(entryPathString(entry));

        ArchiveEntry item;
        item.path = path;
        const auto type = archive_entry_filetype(entry);
        item.isDir = (type == AE_IFDIR);
        item.isSymLink = (type == AE_IFLNK);
        if (item.isSymLink) {
            if (const char *target = archive_entry_symlink(entry)) {
                item.linkTarget = QString::fromUtf8(target);
            }
        }
        if (const char *hardTarget = archive_entry_hardlink(entry)) {
            item.isHardLink = true;
            item.hardLinkTarget = QString::fromUtf8(hardTarget);
        }
        if (archive_entry_size_is_set(entry)) {
            item.size = archive_entry_size(entry);
        }
        const auto mtime = archive_entry_mtime(entry);
        if (mtime > 0) {
            item.mtime = QDateTime::fromSecsSinceEpoch(mtime);
        }
        item.isEncrypted = archive_entry_is_encrypted(entry) != 0;

        if (item.path.isEmpty() || item.path == QStringLiteral(".")) {
            archive_read_data_skip(reader.get());
            continue;
        }

        if (isStreamFamily(m_format) && entries.isEmpty()) {
            // Raw streams either expose the stored name or the placeholder
            // "data"; only the placeholder needs to be replaced.
            // 原始流可能暴露已存储名称或占位符 "data"；仅需替换占位符。
            if (item.path.isEmpty() || item.path == QStringLiteral("data")) {
                item.path = inferStreamName();
            }
            item.isDir = false;
        }

        if (item.isDir && !item.path.endsWith(QLatin1Char('/'))) {
            item.path.append(QLatin1Char('/'));
        }

        entries.append(item);
        archive_read_data_skip(reader.get());
    }

    if (result != ARCHIVE_EOF) {
        const QString message = libarchiveError(reader.get());
        reader.reset();
        reportFailure(message, options.password, tr("Cannot read archive"));
        return;
    }

    m_entries = entries;
    LOG_INFO("LibArchiveBackend", "Listed %d entries from %s",
             static_cast<int>(entries.size()), m_archivePath.toUtf8().constData());
    emit fileListReady(entries);
}

void LibArchiveBackend::extract(const ExtractOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }
    if (options.destDir.trimmed().isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("No destination directory selected"));
        return;
    }

    QSet<QString> selected;
    for (const QString &path : options.selectedFiles) {
        selected.insert(FileUtils::normalizeEntryPath(path));
    }
    const auto isSelected = [&selected](const QString &relative) {
        if (selected.isEmpty()) {
            return true;
        }
        if (selected.contains(relative)) {
            return true;
        }
        for (const QString &candidate : selected) {
            // Selecting a directory selects its subtree (same as passing the
            // member path to 7z), and selecting a child keeps the parent
            // directories needed to recreate the layout.
            // 选中目录会选中其整个子树（与把成员路径传给 7z 相同）；
            // 选中子项时会保留重建目录结构所需的父目录。
            if (relative.startsWith(candidate + QLatin1Char('/'))
                || candidate.startsWith(relative + QLatin1Char('/'))) {
                return true;
            }
        }
        return false;
    };

    const QString tempDir = FileUtils::createTempExtractDir(options.destDir);
    if (tempDir.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::PermissionDenied,
                           tr("Cannot create a temporary directory in %1")
                               .arg(options.destDir));
        return;
    }

    QString error;
    ReaderPtr reader = openReader(m_archivePath, m_format, options.password,
                                  options.encodingHint, &error);
    if (!reader) {
        FileUtils::removeDirectoryRecursively(tempDir);
        reportFailure(error, options.password, tr("Cannot open archive"));
        return;
    }

    // Progress model: prefer the entry sizes from the last listing; fall back
    // to a per-entry counter when the archive was never listed.
    // 进度模型：优先使用最近一次列表中的条目大小；
    // 若从未列表，则回退为按条目计数。
    int totalEntries = 0;
    qint64 totalBytes = 0;
    for (const ArchiveEntry &item : m_entries) {
        QString relative = item.path;
        if (relative.endsWith(QLatin1Char('/'))) {
            relative.chop(1);
        }
        if (!isSelected(relative)) {
            continue;
        }
        ++totalEntries;
        if (!item.isDir) {
            totalBytes += qMax<qint64>(0, item.size);
        }
    }

    QSet<QString> skippedSymlinks;
    for (const QString &path : options.skippedSymlinks) {
        const QString normalized = FileUtils::normalizeEntryPath(path);
        if (!normalized.isEmpty()) {
            skippedSymlinks.insert(normalized);
        }
    }
    QList<QPair<QString, QString>> pendingHardLinks;

    int currentEntry = 0;
    qint64 writtenBytes = 0;
    bool cancelled = false;
    QString failure;

    struct archive_entry *entry = nullptr;
    int result = ARCHIVE_OK;

    while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        if (isCancelled()) {
            cancelled = true;
            break;
        }

        const QString relative = FileUtils::normalizeEntryPath(entryPathString(entry));
        if (relative.isEmpty() || relative == QStringLiteral(".")) {
            archive_read_data_skip(reader.get());
            continue;
        }
        if (!isSelected(relative) || isUnderSkipped(relative, skippedSymlinks)) {
            archive_read_data_skip(reader.get());
            continue;
        }
        if (FileUtils::isPathTraversal(relative, tempDir)) {
            failure = tr("Blocked unsafe path: %1").arg(relative);
            emit errorOccurred(ArchiveErrorCode::PathTraversalBlocked, failure);
            break;
        }

        ++currentEntry;

        const QString outPath = QDir(tempDir).filePath(relative);
        const auto type = archive_entry_filetype(entry);

        if (type == AE_IFDIR) {
            if (!QDir().mkpath(outPath)) {
                failure = tr("Cannot create directory: %1").arg(outPath);
                emit errorOccurred(ArchiveErrorCode::PermissionDenied, failure);
                break;
            }
            continue;
        }

        if (!QDir().mkpath(QFileInfo(outPath).absolutePath())) {
            failure = tr("Cannot create directory: %1")
                          .arg(QFileInfo(outPath).absolutePath());
            emit errorOccurred(ArchiveErrorCode::PermissionDenied, failure);
            break;
        }

        if (type == AE_IFLNK) {
            const char *linkTarget = archive_entry_symlink(entry);
            const QString target = linkTarget ? QString::fromUtf8(linkTarget) : QString();
            if (isUnderSkipped(relative, skippedSymlinks)) {
                LOG_INFO("LibArchiveBackend", "Skipped unsafe symlink by user: %s",
                         relative.toUtf8().constData());
                emit outputReady(tr("Skipped unsafe symlink: %1").arg(relative));
                continue;
            }
            if (!FileUtils::isSafeSymlinkTarget(relative, target)) {
                FileUtils::removeDirectoryRecursively(tempDir);
                emit errorOccurred(ArchiveErrorCode::PathTraversalBlocked,
                                   tr("Blocked unsafe symbolic link: %1").arg(relative));
                return;
            }
#ifdef Q_OS_WIN
            const bool linkCreated = createSymbolicLink(target, outPath);
#else
            const bool linkCreated = QFile::link(target, outPath);
#endif
            if (!linkCreated) {
                FileUtils::removeDirectoryRecursively(tempDir);
                emit errorOccurred(ArchiveErrorCode::PermissionDenied,
                                   tr("Cannot create symbolic link: %1").arg(outPath));
                return;
            }
            continue;
        }

        if (const char *hardTarget = archive_entry_hardlink(entry)) {
            const QString target = FileUtils::normalizeEntryPath(
                QString::fromUtf8(hardTarget));
            if (target.isEmpty()
                || FileUtils::isPathTraversal(target, tempDir)
                || FileUtils::isPathTraversal(relative, tempDir)) {
                FileUtils::removeDirectoryRecursively(tempDir);
                emit errorOccurred(ArchiveErrorCode::HardLinkUnsupported,
                                   tr("Blocked unsafe hard link: %1").arg(relative));
                return;
            }

            const QString targetPath = QDir(tempDir).filePath(target);
            const QFileInfo targetInfo(targetPath);
            if (targetInfo.exists() && !targetInfo.isSymLink()
                && targetInfo.isFile()) {
                if (!createHardLink(targetPath, outPath)) {
                    FileUtils::removeDirectoryRecursively(tempDir);
                    emit errorOccurred(
                        ArchiveErrorCode::HardLinkUnsupported,
                        tr("Cannot create hard link %1 -> %2")
                            .arg(relative, target));
                    return;
                }
            } else {
                // The target may appear later in the archive; defer it.
                // 目标可能稍后才出现在压缩包中；延迟创建。
                pendingHardLinks.append({relative, target});
            }
            continue;
        }

        QFile outFile(outPath);

        if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            failure = tr("Cannot write file: %1").arg(outPath);
            emit errorOccurred(ArchiveErrorCode::PermissionDenied, failure);
            break;
        }

        const void *buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        int readResult = ARCHIVE_OK;

        while ((readResult = archive_read_data_block(reader.get(), &buffer, &size,
                                                    &offset)) == ARCHIVE_OK) {
            if (isCancelled()) {
                cancelled = true;
                break;
            }
            if (offset > outFile.pos() && !outFile.seek(offset)) {
                failure = tr("Cannot write file: %1").arg(outPath);
                break;
            }
            const char *data = static_cast<const char *>(buffer);
            size_t remaining = size;
            while (remaining > 0) {
                const qint64 chunk = outFile.write(data, static_cast<qint64>(remaining));
                if (chunk <= 0) {
                    failure = tr("Cannot write file: %1").arg(outPath);
                    break;
                }
                data += static_cast<qsizetype>(chunk);
                remaining -= static_cast<size_t>(chunk);
                writtenBytes += chunk;
            }
            if (!failure.isEmpty() || cancelled) {
                break;
            }

            int percent = 0;
            if (totalBytes > 0) {
                percent = static_cast<int>(qMin<qint64>(100, writtenBytes * 100 / totalBytes));
            } else if (totalEntries > 0) {
                percent = qMin(100, currentEntry * 100 / totalEntries);
            }
            emit progressChanged(percent, totalEntries, currentEntry, -1);
        }

        outFile.close();

        const auto mtime = archive_entry_mtime(entry);
        if (mtime > 0) {
            QFile written(outPath);
            written.open(QIODevice::ReadWrite);
            written.setFileTime(QDateTime::fromSecsSinceEpoch(mtime),
                                QFileDevice::FileModificationTime);
            written.close();
        }

        if (!failure.isEmpty() || cancelled) {
            break;
        }
        if (readResult != ARCHIVE_EOF) {
            failure = libarchiveError(reader.get());
            // Password failures must go through reportFailure() so the UI is
            // asked for a passphrase instead of seeing a corruption error.
            // 密码错误必须经过 reportFailure()，以便界面提示输入密码，
            // 而不是显示为压缩包损坏。
            reportFailure(failure, options.password, tr("Extraction failed"));
            break;
        }
    }

    if (!pendingHardLinks.isEmpty() && !cancelled && failure.isEmpty()) {
        QList<QPair<QString, QString>> remaining = pendingHardLinks;
        while (!remaining.isEmpty()) {
            QList<QPair<QString, QString>> next;
            bool progress = false;
            for (const auto &[relative, target] : remaining) {
                const QString targetPath = QDir(tempDir).filePath(target);
                const QFileInfo targetInfo(targetPath);
                if (!targetInfo.exists() || targetInfo.isSymLink()
                    || !targetInfo.isFile()) {
                    next.append({relative, target});
                    continue;
                }
                const QString linkPath = QDir(tempDir).filePath(relative);
                if (!createHardLink(targetPath, linkPath)) {
                    FileUtils::removeDirectoryRecursively(tempDir);
                    emit errorOccurred(
                        ArchiveErrorCode::HardLinkUnsupported,
                        tr("Cannot create hard link %1 -> %2")
                            .arg(relative, target));
                    return;
                }
                progress = true;
            }
            if (!progress) {
                FileUtils::removeDirectoryRecursively(tempDir);
                emit errorOccurred(
                    ArchiveErrorCode::HardLinkUnsupported,
                    tr("Hard link target is missing or unsafe: %1")
                        .arg(next.first().second));
                return;
            }
            remaining = next;
        }
    }

    if (cancelled) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (failure.isEmpty() && result != ARCHIVE_EOF) {
        const QString message = libarchiveError(reader.get());
        reader.reset();
        FileUtils::removeDirectoryRecursively(tempDir);
        reportFailure(message, options.password, tr("Extraction failed"));
        return;
    }
    if (!failure.isEmpty()) {
        FileUtils::removeDirectoryRecursively(tempDir);
        return;
    }

    if (!FileUtils::unsafeExtractedSymlinks(tempDir).isEmpty()) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::PathTraversalBlocked,
                           tr("Blocked unsafe symbolic link during extraction"));
        return;
    }

    const int failedMoves = FileUtils::moveExtractedContents(
        tempDir, options.destDir, options.overwrite, options.preservePaths);
    if (failedMoves > 0) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::PartialExtraction,
                           tr("Failed to move %1 extracted entries to %2. "
                              "Files already moved were not rolled back.")
                               .arg(failedMoves)
                               .arg(options.destDir));
        return;
    }

    emit progressChanged(100, totalEntries > 0 ? totalEntries : 1,
                         totalEntries > 0 ? totalEntries : 1, -1);
    emit operationFinished(true, tr("Extraction completed"));
}

void LibArchiveBackend::compress(const CompressOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Creating archives is not supported by the libarchive backend"));
}

void LibArchiveBackend::addFiles(const AddFilesOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Adding files is not supported by the libarchive backend"));
}

void LibArchiveBackend::deleteFiles(const DeleteFilesOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Deleting files is not supported by the libarchive backend"));
}

void LibArchiveBackend::testArchive(const TestOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    QString error;
    ReaderPtr reader = openReader(m_archivePath, m_format, options.password,
                                  QString(), &error);
    if (!reader) {
        reportFailure(error, options.password, tr("Cannot open archive"));
        return;
    }

    struct archive_entry *entry = nullptr;
    int result = ARCHIVE_OK;
    int tested = 0;

    while ((result = archive_read_next_header(reader.get(), &entry)) == ARCHIVE_OK) {
        if (isCancelled()) {
            emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
            return;
        }

        const void *buffer = nullptr;
        size_t size = 0;
        la_int64_t offset = 0;
        int readResult = ARCHIVE_OK;
        while ((readResult = archive_read_data_block(reader.get(), &buffer, &size,
                                                    &offset)) == ARCHIVE_OK) {
            // Data is read (and therefore CRC-checked) but never stored.
            // 读取数据（因此会校验 CRC），但不写入磁盘。
            if (isCancelled()) {
                emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
                return;
            }
        }
        if (readResult != ARCHIVE_EOF) {
            const QString message = libarchiveError(reader.get());
            reader.reset();
            reportFailure(message, options.password, tr("Integrity test failed"));
            return;
        }
        ++tested;
    }

    if (result != ARCHIVE_EOF) {
        const QString message = libarchiveError(reader.get());
        reader.reset();
        reportFailure(message, options.password, tr("Integrity test failed"));
        return;
    }

    LOG_INFO("LibArchiveBackend", "Verified %d entries in %s", tested,
             m_archivePath.toUtf8().constData());
    emit operationFinished(true, tr("Archive integrity verified"));
}

void LibArchiveBackend::closeArchive()
{
    m_isOpen = false;
    m_archivePath.clear();
    m_entries.clear();
}
