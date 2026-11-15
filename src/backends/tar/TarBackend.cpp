#include "TarBackend.h"
#include "TarOutputParser.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>

#include "utils/FileUtils.h"
#include "utils/Logger.h"
#include "utils/ProcessRunner.h"

namespace {

QProcessEnvironment tarEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Stable, parseable verbose output and English error messages.  File name
    // bytes are passed through untouched; decoding is handled separately.
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    env.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    return env;
}

QString commonParentDirectory(const QStringList &files)
{
    if (files.isEmpty()) {
        return QString();
    }

    auto isUnder = [](const QString &path, const QString &base) {
        if (base == QStringLiteral("/")) {
            return path.startsWith(QLatin1Char('/'));
        }
        return path == base || path.startsWith(base + QLatin1Char('/'));
    };

    QString common = QFileInfo(files.first()).absolutePath();
    for (const QString &file : files) {
        QString dir = QFileInfo(file).absolutePath();
        while (!isUnder(dir, common)) {
            const QString parent = QFileInfo(common).absolutePath();
            if (parent == common) {
                break;
            }
            common = parent;
        }
    }
    return common;
}

} // namespace

TarBackend::TarBackend(ArchiveFormat fmt, QObject *parent)
    : ArchiveBackend(parent)
    , m_format(fmt)
{
}

QString TarBackend::decompressFlag() const
{
    switch (m_format) {
    case ArchiveFormat::TarGz:  return QStringLiteral("-z");
    case ArchiveFormat::TarBz2: return QStringLiteral("-j");
    case ArchiveFormat::TarXz:  return QStringLiteral("-J");
    case ArchiveFormat::Tar:    return QString();
    default:                    return QString();
    }
}

QString TarBackend::compressFlag(ArchiveFormat fmt) const
{
    switch (fmt) {
    case ArchiveFormat::TarGz:  return QStringLiteral("-z");
    case ArchiveFormat::TarBz2: return QStringLiteral("-j");
    case ArchiveFormat::TarXz:  return QStringLiteral("-J");
    case ArchiveFormat::Tar:    return QString();
    default:                    return QString();
    }
}

int TarBackend::validateCompressionLevel(int level) const
{
    if (m_format == ArchiveFormat::Tar) {
        return -1;
    }
    int minLevel = 0;
    if (m_format == ArchiveFormat::TarGz || m_format == ArchiveFormat::TarBz2) {
        minLevel = 1;
    }
    if (level < minLevel || level > 9) {
        return -1;
    }
    return level;
}

QStringList TarBackend::excludeArgs(const QStringList &patterns) const
{
    QStringList args;
    for (const QString &pattern : patterns) {
        if (!pattern.trimmed().isEmpty()) {
            args << QStringLiteral("--exclude") << pattern;
        }
    }
    return args;
}

void TarBackend::openArchive(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists()) {
        emit errorOccurred(ArchiveErrorCode::FileNotFound,
                           tr("File does not exist: %1").arg(filePath));
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

void TarBackend::listContents(const ListOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    const QString tarPath = ProcessRunner::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("-t") << QStringLiteral("-v");
    // Do not let tar octal-escape non-ASCII file names (LC_ALL=C would
    // otherwise turn UTF-8 names into \NNN sequences).  The names must match
    // the raw member names when the user later extracts a selected entry.
    args << QStringLiteral("--quoting-style=literal");
    if (!decompressFlag().isEmpty()) {
        args << decompressFlag();
    }
    args << QStringLiteral("-f") << m_archivePath;

    const ProcessResult result = ProcessRunner::run(
        tarPath, args, options.encodingHint, tarEnvironment(),
        QString(), 300000, &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.started) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start tar"));
        return;
    }
    if (!result.success()) {
        const QString errorText = result.stdErr.trimmed();
        if (errorText.contains(QStringLiteral("not look like a tar archive"), Qt::CaseInsensitive)
            || errorText.contains(QStringLiteral("Unrecognized archive format"), Qt::CaseInsensitive)
            || errorText.contains(QStringLiteral("unexpected EOF"), Qt::CaseInsensitive)) {
            emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                               tr("This does not look like a tar archive: %1").arg(m_archivePath));
        } else {
            emit errorOccurred(ArchiveErrorCode::Unknown,
                               errorText.isEmpty() ? tr("Failed to list archive") : errorText);
        }
        return;
    }

    m_entries = TarOutputParser::parseList(result.stdOut);
    emit fileListReady(m_entries);
}

void TarBackend::extract(const ExtractOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    const QString tarPath = ProcessRunner::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
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
    args << QStringLiteral("-x") << QStringLiteral("-v");
    if (!decompressFlag().isEmpty()) {
        args << decompressFlag();
    }
    args << QStringLiteral("-f") << m_archivePath
         << QStringLiteral("-C") << tempDir;

    for (const QString &selected : options.selectedFiles) {
        const QString normalized = FileUtils::normalizeEntryPath(selected);
        if (!normalized.isEmpty()) {
            // Pass the member name exactly as listed by tar.  Normalizing it
            // would strip a leading "./" or alter literal backslashes and
            // make tar report "Not found in archive".
            args << selected;
        }
    }

    LOG_DEBUG("TarBackend", "Extract: %s",
              ProcessRunner::maskedCommand(tarPath, args).toUtf8().constData());

    const int total = options.selectedFiles.isEmpty()
        ? m_entries.size()
        : options.selectedFiles.size();
    int processed = 0;

    const ProcessResult result = ProcessRunner::run(
        tarPath, args, options.encodingHint, tarEnvironment(), QString(), 0,
        &m_cancelRequested,
        [this, total, &processed](const QString &line, bool isError) {
            if (isError || line.trimmed().isEmpty()) {
                return;
            }
            emit outputReady(line);
            ++processed;
            const int percent = total > 0
                ? qBound(0, processed * 100 / total, 100)
                : 0;
            emit progressChanged(percent, total, processed, -1);
        });

    if (result.cancelled) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.started) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start tar"));
        return;
    }
    if (!result.success()) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Extraction failed: %1").arg(result.stdErr.trimmed()));
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

void TarBackend::compress(const CompressOptions &options)
{
    const QString tarPath = ProcessRunner::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    if (!options.password.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("tar does not support encryption; use 7z or zip instead"));
        return;
    }

    const int level = validateCompressionLevel(options.compressionLevel);

    QStringList args;
    args << QStringLiteral("-c") << QStringLiteral("-v");

    if (m_format != ArchiveFormat::Tar) {
        if (level > 0) {
            QString program;
            switch (m_format) {
            case ArchiveFormat::TarGz:  program = QStringLiteral("gzip -%1").arg(level); break;
            case ArchiveFormat::TarBz2: program = QStringLiteral("bzip2 -%1").arg(level); break;
            case ArchiveFormat::TarXz:  program = QStringLiteral("xz -T0 -%1").arg(level); break;
            default: break;
            }
            args << QStringLiteral("--use-compress-program") << program;
        } else if (!compressFlag(m_format).isEmpty()) {
            args << compressFlag(m_format);
        }
    }

    if (options.followSymlinks) {
        args << QStringLiteral("-h");
    }

    args << excludeArgs(options.excludePatterns);

    QString baseDir;
    QStringList relativeFiles;

    if (options.storeFullPath) {
        args << QStringLiteral("-P");
        relativeFiles = options.sourceFiles;
    } else {
        baseDir = commonParentDirectory(options.sourceFiles);
        for (const QString &file : options.sourceFiles) {
            relativeFiles << QDir(baseDir).relativeFilePath(file);
        }
    }

    args << QStringLiteral("-f") << options.archivePath;
    if (!baseDir.isEmpty()) {
        args << QStringLiteral("-C") << baseDir;
    }
    args.append(relativeFiles);

    LOG_DEBUG("TarBackend", "Compress: %s",
              ProcessRunner::maskedCommand(tarPath, args).toUtf8().constData());

    const int total = FileUtils::countFiles(options.sourceFiles);
    int processed = 0;

    const ProcessResult result = ProcessRunner::run(
        tarPath, args, QString(), tarEnvironment(), QString(), 0,
        &m_cancelRequested,
        [this, total, &processed](const QString &line, bool isError) {
            if (isError || line.trimmed().isEmpty()) {
                return;
            }
            emit outputReady(line);
            ++processed;
            const int percent = total > 0
                ? qBound(0, processed * 100 / total, 100)
                : 0;
            emit progressChanged(percent, total, processed, -1);
        });

    if (result.cancelled) {
        QFile::remove(options.archivePath);
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.started) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start tar"));
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

void TarBackend::addFiles(const AddFilesOptions &options)
{
    if (!supportsAppend()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("Cannot add files to compressed tar archives"));
        return;
    }

    const QString tarPath = ProcessRunner::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("-r") << QStringLiteral("-v");
    if (options.storeFullPath) {
        args << QStringLiteral("-P");
    }
    args << QStringLiteral("-f") << options.archivePath;
    args.append(options.sourceFiles);

    const ProcessResult result = ProcessRunner::run(
        tarPath, args, QString(), tarEnvironment(), QString(), 300000,
        &m_cancelRequested);

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

void TarBackend::deleteFiles(const DeleteFilesOptions &options)
{
    if (!supportsAppend()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("Cannot delete files from compressed tar archives"));
        return;
    }

    const QString tarPath = ProcessRunner::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("--delete") << QStringLiteral("-v")
         << QStringLiteral("-f") << options.archivePath;
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
        tarPath, args, QString(), tarEnvironment(), QString(), 300000,
        &m_cancelRequested);

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

void TarBackend::testArchive(const TestOptions &options)
{
    Q_UNUSED(options);

    const QString tarPath = ProcessRunner::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("-t");
    if (!decompressFlag().isEmpty()) {
        args << decompressFlag();
    }
    args << QStringLiteral("-f") << m_archivePath;

    const ProcessResult result = ProcessRunner::run(
        tarPath, args, QString(), tarEnvironment(), QString(), 300000,
        &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.success()) {
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                           tr("Archive test failed (corrupted)"));
        return;
    }
    emit operationFinished(true, tr("Archive integrity verified"));
}

void TarBackend::closeArchive()
{
    m_isOpen = false;
    m_archivePath.clear();
    m_entries.clear();
}

bool TarBackend::supportsAppend() const
{
    return m_format == ArchiveFormat::Tar;
}
