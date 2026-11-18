// QArchiver / 小Q解压
// 7-Zip command construction, listing, extraction, writing and splits.
// 7-Zip 命令构建、列表、解压、写入及分卷处理。

#include "SevenZipBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#include <algorithm>

#include "SevenZipListParser.h"
#include "SevenZipUtils.h"
#include "utils/EncodingDetector.h"
#include "utils/FileUtils.h"
#include "utils/Logger.h"
#include "utils/MimeTypeHelper.h"
#include "utils/ProcessRunner.h"

SevenZipBackend::SevenZipBackend(ArchiveFormat fmt, QObject *parent)
    : ArchiveBackend(parent)
    , m_format(fmt)
{
}

QString SevenZipBackend::sevenZipPath() const
{
    return ProcessRunner::resolveToolPath(QStringLiteral("7z"));
}

QString SevenZipBackend::formatSwitch() const
{
    return m_format == ArchiveFormat::Zip ? QStringLiteral("-tzip") : QString();
}

bool SevenZipBackend::isSplitArchive() const
{
    return MimeTypeHelper::isSplitVolume(m_archivePath);
}

QString SevenZipBackend::targetPath() const
{
    return isSplitArchive() ? MimeTypeHelper::resolveFirstVolume(m_archivePath)
                            : m_archivePath;
}

void SevenZipBackend::appendPassword(QStringList &args, const QString &password) const
{
    // Always pass -p, even when the password is empty.  Without it, 7-Zip
    // prompts for a password on stdin when the archive has encrypted headers
    // and the child process would block forever.
    // 即使密码为空也始终传递 -p。否则当压缩包加密了文件头时，
    // 7-Zip 会从 stdin 提示输入密码，导致子进程永久阻塞。
    args << QStringLiteral("-p") + password;
}

int SevenZipBackend::validateLevel(int level) const
{
    return qBound(0, level, 9);
}

void SevenZipBackend::openArchive(const QString &filePath)
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
    m_encrypted = false;
    m_nameEncoding = QStringLiteral("UTF-8");
    m_isOpen = true;
    emit archiveOpened(filePath);
}

void SevenZipBackend::listContents(const ListOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    const QString sevenZip = sevenZipPath();
    if (sevenZip.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("l") << QStringLiteral("-slt")
         << QStringLiteral("-sccUTF-8");
    appendPassword(args, options.password);
    args << QStringLiteral("--") << targetPath();

    const ProcessResult result = ProcessRunner::run(
        sevenZip, args, QString(), QProcessEnvironment(),
        QString(), 300000, &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.started) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start 7z"));
        return;
    }

    if (!result.success()) {
        const QString errorText = result.stdErr;
        // 7-Zip reports encrypted headers as "Cannot open encrypted
        // archive. Wrong password?"; a plain "Can not open" is usually a
        // corrupted/missing archive, so do not turn every open failure into a
        // password prompt.
        // 7-Zip 对加密文件头会报告 "Cannot open encrypted archive. Wrong
        // password?"；普通的 "Can not open" 通常是压缩包损坏或缺失，
        // 因此不能把所有打开失败都当成密码请求。
        const bool looksLikePassword = errorText.contains(QStringLiteral("password"), Qt::CaseInsensitive)
            || errorText.contains(QStringLiteral("encrypted archive"), Qt::CaseInsensitive);

        if (looksLikePassword) {
            m_encrypted = true;
            if (options.password.isEmpty()) {
                emit passwordRequested();
                return;
            }
            emit errorOccurred(ArchiveErrorCode::WrongPassword, tr("Wrong password"));
            return;
        }

        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("List failed: %1").arg(errorText.trimmed()));
        return;
    }

    m_entries = SevenZipListParser::parse(result.stdOut);
    m_nameEncoding = EncodingDetector::detect(result.stdOutRaw);
    m_encrypted = std::any_of(m_entries.cbegin(), m_entries.cend(),
                              [](const ArchiveEntry &entry) { return entry.isEncrypted; });
    emit fileListReady(m_entries);
}

void SevenZipBackend::extract(const ExtractOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    const QString sevenZip = sevenZipPath();
    if (sevenZip.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    for (const QString &selected : options.selectedFiles) {
        if (FileUtils::isPathTraversal(selected, options.destDir)) {
            emit errorOccurred(ArchiveErrorCode::PathTraversalBlocked,
                               tr("Blocked unsafe path: %1").arg(selected));
            return;
        }
    }

    QDir().mkpath(options.destDir);
    const QString tempDir = FileUtils::createTempExtractDir(options.destDir);

    QStringList args;
    args << QStringLiteral("x")
         << QStringLiteral("-o") + tempDir
         << QStringLiteral("-y")
         << QStringLiteral("-spd")
         << QStringLiteral("-sccUTF-8");
    appendPassword(args, options.password);

    // Pass selected members through a listfile instead of the command line:
    // 7-Zip interprets positional names as wildcards, treats leading '-' as a
    // switch and leading '@' as a nested listfile.
    // 通过列表文件而不是命令行传递选定成员：7-Zip 会把位置参数当作通配符，
    // 把前导 '-' 当作开关，把前导 '@' 当作嵌套列表文件。
    QTemporaryFile membersFile(
        QDir::tempPath() + QStringLiteral("/qarchiver-7z-members-XXXXXX.txt"));
    if (!options.selectedFiles.isEmpty()) {
        QStringList selectedPaths;
        selectedPaths.reserve(options.selectedFiles.size());
        for (const QString &selected : options.selectedFiles) {
            const QString normalized = FileUtils::normalizeEntryPath(selected);
            if (!normalized.isEmpty()) {
                selectedPaths << selected;
            }
        }
        if (selectedPaths.isEmpty()
            || !SevenZipUtils::appendListFileArgs(membersFile, selectedPaths,
                                   m_nameEncoding, args)) {
            FileUtils::removeDirectoryRecursively(tempDir);
            emit errorOccurred(ArchiveErrorCode::Unknown,
                               tr("Could not prepare the selected files for extraction"));
            return;
        }
    }

    args << QStringLiteral("--") << targetPath();

    LOG_DEBUG("SevenZipBackend", "Extract: %s",
              ProcessRunner::maskedCommand(sevenZip, args).toUtf8().constData());

    const int total = options.selectedFiles.isEmpty()
        ? m_entries.size()
        : options.selectedFiles.size();
    int processed = 0;

    const ProcessResult result = ProcessRunner::run(
        sevenZip, args, QString(), QProcessEnvironment(),
        QString(), 0, &m_cancelRequested,
        [this, total, &processed](const QString &line, bool isError) {
            if (isError || line.isEmpty()) {
                return;
            }
            emit outputReady(line);
            if (line.startsWith(QLatin1String("- "))
                || line.startsWith(QLatin1String("+ "))
                || line.startsWith(QLatin1String("U "))) {
                ++processed;
                const int percent = total > 0
                    ? qBound(0, processed * 100 / total, 100)
                    : 0;
                emit progressChanged(percent, total, processed, -1);
            }
        });

    if (result.cancelled) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.started) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start 7z"));
        return;
    }
    if (!result.success()) {
        FileUtils::removeDirectoryRecursively(tempDir);
        const QString errorText = result.stdErr;
        if (m_encrypted
            && (errorText.contains(QStringLiteral("Wrong password"), Qt::CaseInsensitive)
                || errorText.contains(QStringLiteral("Data Error"), Qt::CaseInsensitive))) {
            emit errorOccurred(ArchiveErrorCode::WrongPassword, tr("Wrong password"));
        } else {
            emit errorOccurred(ArchiveErrorCode::Unknown,
                               tr("Extraction failed: %1").arg(errorText.trimmed()));
        }
        return;
    }

#ifndef Q_OS_WIN
    // Convert legacy-codepage filenames written by 7-Zip into UTF-8 before the
    // temporary tree is moved to its final destination.
    // 在临时目录移动到最终位置前，将 7-Zip 写出的旧代码页文件名转换为 UTF-8。
    if (m_nameEncoding.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) != 0) {
        SevenZipUtils::renameLegacyExtractedTree(QFile::encodeName(tempDir), m_nameEncoding);
    }
#endif

    const int failedMoves = FileUtils::moveExtractedContents(
        tempDir, options.destDir, options.overwrite, options.preservePaths);
    if (failedMoves > 0) {
        emit operationFinished(false,
            tr("Extraction completed, but %1 entr%2 could not be moved")
                .arg(failedMoves)
                .arg(failedMoves == 1 ? QStringLiteral("y") : QStringLiteral("ies")));
        return;
    }

    emit progressChanged(100, total, total, -1);
    emit operationFinished(true, tr("Extraction completed"));
}

void SevenZipBackend::compress(const CompressOptions &options)
{
    if (isReadOnly()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("This archive format is read-only"));
        return;
    }

    const QString sevenZip = sevenZipPath();
    if (sevenZip.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("a");
    if (!formatSwitch().isEmpty()) {
        args << formatSwitch();
    }
    args << QStringLiteral("-mx=%1").arg(validateLevel(options.compressionLevel))
         << QStringLiteral("-mmt=on")
         << QStringLiteral("-sccUTF-8");

    if (m_format == ArchiveFormat::SevenZip && options.solidMode) {
        args << QStringLiteral("-ms=on");
    }
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
        if (m_format == ArchiveFormat::SevenZip && options.encryptHeaders) {
            args << QStringLiteral("-mhe=on");
        }
    }
    if (options.volumeSize > 0) {
        args << QStringLiteral("-v%1b").arg(options.volumeSize);
    }
    // 7-Zip follows symlinks by default; -snl stores them as links.
    // 7-Zip 默认跟随符号链接；-snl 将符号链接按链接本身保存。
    if (!options.followSymlinks) {
        args << QStringLiteral("-snl");
    }
    if (options.storeFullPath) {
        args << QStringLiteral("-spf");
    }
    for (const QString &pattern : options.excludePatterns) {
        if (!pattern.trimmed().isEmpty()) {
            args << QStringLiteral("-x!") + pattern;
        }
    }

    QTemporaryFile sourceListFile(
        QDir::tempPath() + QStringLiteral("/qarchiver-7z-sources-XXXXXX.txt"));
    if (!SevenZipUtils::appendListFileArgs(sourceListFile, options.sourceFiles,
                            QStringLiteral("UTF-8"), args)) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Could not prepare the source files for compression"));
        return;
    }

    // -spd disables wildcard matching for both filenames and -x patterns, so
    // only enable it when no exclude patterns are in use.
    // -spd 会同时禁用文件名与 -x 模式的通配符匹配，
    // 因此仅在没有排除规则时启用。
    if (options.excludePatterns.isEmpty()) {
        args << QStringLiteral("-spd");
    }

    args << QStringLiteral("--") << options.archivePath;

    LOG_DEBUG("SevenZipBackend", "Compress: %s",
              ProcessRunner::maskedCommand(sevenZip, args).toUtf8().constData());

    const int total = FileUtils::countFiles(options.sourceFiles);
    int processed = 0;

    const ProcessResult result = ProcessRunner::run(
        sevenZip, args, QString(), QProcessEnvironment(),
        QString(), 0, &m_cancelRequested,
        [this, total, &processed](const QString &line, bool isError) {
            if (isError || line.isEmpty()) {
                return;
            }
            emit outputReady(line);
            if (line.startsWith(QLatin1String("+ "))) {
                ++processed;
                const int percent = total > 0
                    ? qBound(0, processed * 100 / total, 100)
                    : 0;
                emit progressChanged(percent, total, processed, -1);
            }
        });

    if (result.cancelled) {
        QFile::remove(options.archivePath);
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.started) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start 7z"));
        return;
    }
    if (!result.success()) {
        QFile::remove(options.archivePath);
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Compression failed: %1").arg(result.stdErr.trimmed()));
        return;
    }

    emit progressChanged(100, total, processed, -1);
    emit operationFinished(true, tr("Archive created: %1").arg(options.archivePath));
}

void SevenZipBackend::addFiles(const AddFilesOptions &options)
{
    if (isReadOnly()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("This archive format is read-only"));
        return;
    }
    if (!supportsAppend()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("Cannot add files to a split archive"));
        return;
    }

    const QString sevenZip = sevenZipPath();
    if (sevenZip.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("a");
    if (!formatSwitch().isEmpty()) {
        args << formatSwitch();
    }
    args << QStringLiteral("-mx=%1").arg(validateLevel(options.compressionLevel))
         << QStringLiteral("-sccUTF-8");
    if (m_encrypted && options.password.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::PasswordRequired,
                           tr("Password required"));
        return;
    }
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }
    // 7-Zip follows symlinks by default; -snl stores them as links.
    if (!options.followSymlinks) {
        args << QStringLiteral("-snl");
    }
    if (options.storeFullPath) {
        args << QStringLiteral("-spf");
    }

    QTemporaryFile sourceListFile(
        QDir::tempPath() + QStringLiteral("/qarchiver-7z-sources-XXXXXX.txt"));
    if (!SevenZipUtils::appendListFileArgs(sourceListFile, options.sourceFiles,
                            QStringLiteral("UTF-8"), args)) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Could not prepare the source files for adding"));
        return;
    }
    args << QStringLiteral("-spd")
         << QStringLiteral("--") << options.archivePath;

    const ProcessResult result = ProcessRunner::run(
        sevenZip, args, QString(), QProcessEnvironment(),
        QString(), 300000, &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.success()) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Add files failed: %1").arg(result.stdErr.trimmed()));
        return;
    }
    emit operationFinished(true, tr("Files added to archive"));
}

void SevenZipBackend::deleteFiles(const DeleteFilesOptions &options)
{
    if (isReadOnly()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("This archive format is read-only"));
        return;
    }
    if (!supportsAppend()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("Cannot delete files from a split archive"));
        return;
    }

    const QString sevenZip = sevenZipPath();
    if (sevenZip.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("d");
    if (!formatSwitch().isEmpty()) {
        args << formatSwitch();
    }
    args << QStringLiteral("-spd") << QStringLiteral("-sccUTF-8");
    if (m_encrypted && options.password.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::PasswordRequired,
                           tr("Password required"));
        return;
    }
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }

    QStringList targetEntries;
    targetEntries.reserve(options.targetEntries.size());
    for (const QString &entry : options.targetEntries) {
        const QString normalized = FileUtils::normalizeEntryPath(entry);
        if (normalized.isEmpty()) {
            continue;
        }
        if (normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts)
                .contains(QStringLiteral(".."))) {
            emit errorOccurred(ArchiveErrorCode::PathTraversalBlocked,
                               tr("Blocked unsafe path: %1").arg(entry));
            return;
        }
        targetEntries << entry;
    }

    QTemporaryFile membersFile(
        QDir::tempPath() + QStringLiteral("/qarchiver-7z-members-XXXXXX.txt"));
    if (targetEntries.isEmpty()
        || !SevenZipUtils::appendListFileArgs(membersFile, targetEntries,
                               m_nameEncoding, args)) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Could not prepare the selected files for deletion"));
        return;
    }

    args << QStringLiteral("--") << options.archivePath;

    const ProcessResult result = ProcessRunner::run(
        sevenZip, args, QString(), QProcessEnvironment(),
        QString(), 300000, &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.success()) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Delete files failed: %1").arg(result.stdErr.trimmed()));
        return;
    }
    emit operationFinished(true, tr("Files deleted from archive"));
}

void SevenZipBackend::testArchive(const TestOptions &options)
{
    const QString sevenZip = sevenZipPath();
    if (sevenZip.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("t") << QStringLiteral("-sccUTF-8");
    appendPassword(args, options.password);
    args << QStringLiteral("--") << targetPath();

    const ProcessResult result = ProcessRunner::run(
        sevenZip, args, QString(), QProcessEnvironment(),
        QString(), 300000, &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.success()) {
        if (m_encrypted
            && result.stdErr.contains(QStringLiteral("Wrong password"), Qt::CaseInsensitive)) {
            emit errorOccurred(ArchiveErrorCode::WrongPassword, tr("Wrong password"));
        } else {
            emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                               tr("Archive test failed"));
        }
        return;
    }
    emit operationFinished(true, tr("Archive integrity verified"));
}

void SevenZipBackend::closeArchive()
{
    m_isOpen = false;
    m_archivePath.clear();
    m_entries.clear();
    m_encrypted = false;
    m_nameEncoding = QStringLiteral("UTF-8");
}

bool SevenZipBackend::supportsAppend() const
{
    return m_isOpen && !isSplitArchive();
}
