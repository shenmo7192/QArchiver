#include "SevenZipBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <algorithm>

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
    if (!password.isEmpty()) {
        args << QStringLiteral("-p") + password;
    }
}

int SevenZipBackend::validateLevel(int level) const
{
    return qBound(0, level, 9);
}

QList<ArchiveEntry> SevenZipBackend::parseList(const QString &output) const
{
    // Parse 7z `l -slt` (technical) output.  The archive header block and each
    // entry are "Key = Value" line groups; a new entry starts with "Path =".
    // A block is an actual entry when it contains "Folder" (zip) or
    // "Attributes" (7z).
    QList<ArchiveEntry> entries;
    QString data = output;
    QTextStream stream(&data);
    stream.setEncoding(QStringConverter::Utf8);
    QString line;
    ArchiveEntry current;
    bool currentIsEntry = false;

    auto commitEntry = [&]() {
        if (currentIsEntry && !current.path.isEmpty()) {
            entries.append(current);
        }
        current = ArchiveEntry{};
        currentIsEntry = false;
    };

    while (stream.readLineInto(&line)) {
        line = line.trimmed();
        const int equals = line.indexOf(QLatin1Char('='));
        if (equals < 0) {
            continue;
        }
        const QString key = line.left(equals).trimmed();
        const QString value = line.mid(equals + 1).trimmed();

        if (key == QLatin1String("Path")) {
            commitEntry();
            current.path = value;
        } else if (key == QLatin1String("Folder")) {
            current.isDir = (value == QLatin1String("+"));
            currentIsEntry = true;
        } else if (key == QLatin1String("Attributes")) {
            current.isDir = value.startsWith(QLatin1Char('D'));
            currentIsEntry = true;
        } else if (key == QLatin1String("Size")) {
            current.size = value.toLongLong();
        } else if (key == QLatin1String("Packed Size")) {
            current.packedSize = value.toLongLong();
        } else if (key == QLatin1String("Modified")) {
            current.mtime = QDateTime::fromString(value.left(19),
                                                  QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        } else if (key == QLatin1String("Encrypted")) {
            current.isEncrypted = (value == QLatin1String("+"));
        }
    }
    commitEntry();
    return entries;
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
    args << QStringLiteral("l") << targetPath() << QStringLiteral("-slt");
    appendPassword(args, options.password);

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
        const bool looksLikePassword = errorText.contains(QStringLiteral("password"), Qt::CaseInsensitive)
            || errorText.contains(QStringLiteral("Can not open"), Qt::CaseInsensitive)
            || errorText.contains(QStringLiteral("Cannot open"), Qt::CaseInsensitive);

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

    m_entries = parseList(result.stdOut);
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
    args << QStringLiteral("x") << targetPath()
         << QStringLiteral("-o") + tempDir << QStringLiteral("-y");
    appendPassword(args, options.password);

    for (const QString &selected : options.selectedFiles) {
        const QString normalized = FileUtils::normalizeEntryPath(selected);
        if (!normalized.isEmpty()) {
            // Use the exact member path.  A leading "./" or a literal
            // backslash is significant to the archive tool.
            args << selected;
        }
    }

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
    args << options.archivePath
         << QStringLiteral("-mx=%1").arg(validateLevel(options.compressionLevel))
         << QStringLiteral("-mmt=on");

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
    if (options.followSymlinks) {
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

    args.append(options.sourceFiles);

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
    args << options.archivePath
         << QStringLiteral("-mx=%1").arg(validateLevel(options.compressionLevel));
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }
    if (options.followSymlinks) {
        args << QStringLiteral("-snl");
    }
    if (options.storeFullPath) {
        args << QStringLiteral("-spf");
    }
    args.append(options.sourceFiles);

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
    args << options.archivePath;
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }

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
        args << entry;
    }

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
    args << QStringLiteral("t") << targetPath();
    appendPassword(args, options.password);

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
}

bool SevenZipBackend::supportsAppend() const
{
    return m_isOpen && !isSplitArchive();
}
