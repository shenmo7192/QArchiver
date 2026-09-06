#include "TarBackend.h"
#include "TarOutputParser.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QUuid>
#include <QProcessEnvironment>
#include <QTimer>

#include "utils/Logger.h"
#include "utils/FileUtils.h"
#include "utils/EncodingDetector.h"
#include "utils/ProcessHelper.h"
#include "core/ArchiveOptions.h"

TarBackend::TarBackend(ArchiveFormat fmt, QObject *parent)
    : ArchiveBackend(parent)
    , m_format(fmt)
{
    m_process = new ProcessHelper(this);
    connect(m_process, &ProcessHelper::outputLine, this, &ArchiveBackend::outputReady);
    connect(m_process, &ProcessHelper::progress, this, [this](const QString &line){
        // Count processed entries for progress
        static int processed = 0;
        if (!line.isEmpty()) {
            ++processed;
            int total = m_entries.size();
            int percent = total > 0 ? (processed * 100 / total) : 0;
            emit progressChanged(percent, total, processed, -1);
        }
    });
}

QString TarBackend::decompressFlag() const
{
    switch (m_format) {
    case ArchiveFormat::TarGz:  return QStringLiteral("-z");
    case ArchiveFormat::TarBz2: return QStringLiteral("-j");
    case ArchiveFormat::TarXz:  return QStringLiteral("-J");
    case ArchiveFormat::Tar:    return QString();
    default: return QString();
    }
}

QString TarBackend::compressFlag(ArchiveFormat fmt) const
{
    switch (fmt) {
    case ArchiveFormat::TarGz:  return QStringLiteral("-z");
    case ArchiveFormat::TarBz2: return QStringLiteral("-j");
    case ArchiveFormat::TarXz:  return QStringLiteral("-J");
    case ArchiveFormat::Tar:    return QString();
    default: return QString();
    }
}

int TarBackend::validateCompressionLevel(int level) const
{
    // tar ignores level for plain .tar
    if (m_format == ArchiveFormat::Tar) {
        return -1; // signal to omit
    }
    // gzip/bzip2: valid [1,9]; xz: valid [0,9]
    int minLevel = 0;
    if (m_format == ArchiveFormat::TarGz || m_format == ArchiveFormat::TarBz2) {
        minLevel = 1;
    }
    if (level < minLevel || level > 9) {
        LOG_INFO("TarBackend", "Compression level %d invalid for format; using tool default", level);
        return -1; // omit, use tool default
    }
    return level;
}

QStringList TarBackend::excludeArgs(const QStringList &patterns) const
{
    QStringList args;
    for (const QString &p : patterns) {
        args << QStringLiteral("--exclude") << p;
    }
    return args;
}

void TarBackend::openArchive(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        emit errorOccurred(ArchiveErrorCode::FileNotFound,
            tr("File does not exist: %1").arg(filePath));
        return;
    }
    if (!fi.isReadable()) {
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

    const QString tarPath = ProcessHelper::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    // Set locale for encoding if hint provided
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!options.encodingHint.isEmpty()) {
        if (options.encodingHint.compare(QStringLiteral("GBK"), Qt::CaseInsensitive) == 0) {
            env.insert(QStringLiteral("LANG"), QStringLiteral("zh_CN.GBK"));
            env.insert(QStringLiteral("LC_ALL"), QStringLiteral("zh_CN.GBK"));
        }
    }

    QStringList args;
    args << QStringLiteral("-t") << QStringLiteral("-v")
         << decompressFlag()
         << QStringLiteral("-f") << m_archivePath;

    // Capture full output synchronously in worker thread
    QProcess proc;
    proc.setProcessEnvironment(env);
    proc.start(tarPath, args);
    if (!proc.waitForStarted(5000)) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start tar"));
        return;
    }
    // Read all output (may be large)
    QByteArray output;
    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(30000);
        output += proc.readAllStandardOutput();
        if (proc.state() == QProcess::NotRunning) break;
    }

    // Handle stderr for errors
    QByteArray err = proc.readAllStandardError();
    if (proc.exitCode() != 0) {
        QString errMsg = QString::fromLocal8Bit(err);
        if (errMsg.contains(QStringLiteral("Unrecognized archive format"), Qt::CaseInsensitive)) {
            emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                tr("This does not look like a tar archive: %1").arg(m_archivePath));
        } else {
            emit errorOccurred(ArchiveErrorCode::Unknown, errMsg);
        }
        return;
    }

    // Decode and parse
    QString decoded = QString::fromLocal8Bit(output);
    m_entries = TarOutputParser::parseList(decoded);
    emit fileListReady(m_entries);
}

void TarBackend::extract(const ExtractOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    const QString tarPath = ProcessHelper::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    // Validate destination
    QDir destDir(options.destDir);
    if (!destDir.exists()) {
        QDir().mkpath(options.destDir);
    }

    // Create temp extraction dir
    const QString tempDir = FileUtils::createTempExtractDir(options.destDir);

    // Set encoding environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!options.encodingHint.isEmpty() &&
        options.encodingHint.compare(QStringLiteral("GBK"), Qt::CaseInsensitive) == 0) {
        env.insert(QStringLiteral("LANG"), QStringLiteral("zh_CN.GBK"));
        env.insert(QStringLiteral("LC_ALL"), QStringLiteral("zh_CN.GBK"));
    }

    QStringList args;
    args << QStringLiteral("-x") << QStringLiteral("-v")
         << decompressFlag()
         << QStringLiteral("-f") << m_archivePath
         << QStringLiteral("-C") << tempDir;

    // Add selected files (normalized)
    for (const QString &sel : options.selectedFiles) {
        QString norm = FileUtils::normalizeEntryPath(sel);
        if (!norm.isEmpty() && !norm.contains("..")) {
            args << norm;
        }
    }

    LOG_DEBUG("TarBackend", "Extract: %s", ProcessHelper::maskedCommand(tarPath, args).toUtf8().constData());

    QProcess proc;
    proc.setProcessEnvironment(env);
    proc.start(tarPath, args);
    if (!proc.waitForStarted(5000)) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start tar"));
        return;
    }

    int processed = 0;
    const int total = options.selectedFiles.isEmpty() ? m_entries.size() : options.selectedFiles.size();

    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(30000);
        QByteArray data = proc.readAllStandardOutput();
        if (!data.isEmpty()) {
            emit outputReady(QString::fromLocal8Bit(data).trimmed());
            processed = TarOutputParser::countProcessed(QString::fromLocal8Bit(data));
            int percent = total > 0 ? (processed * 100 / total) : 0;
            emit progressChanged(percent, total, processed, -1);
        }
        if (proc.state() == QProcess::NotRunning) break;
    }

    if (proc.exitCode() != 0) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::Unknown,
            tr("Extraction failed: %1").arg(QString::fromLocal8Bit(proc.readAllStandardError())));
        return;
    }

    // Move everything from the temp dir into destDir. Directories are
    // recreated with mkpath and only files/symlinks are renamed, which avoids
    // the "directory moved first -> children's source paths disappear" bug.
    const int failedMoves = FileUtils::moveExtractedContents(tempDir, options.destDir,
                                                             options.overwrite);
    if (failedMoves > 0) {
        LOG_WARN("TarBackend", "Failed to move %d extracted entr%s",
                 failedMoves, failedMoves == 1 ? "y" : "ies");
    }

    emit operationFinished(true, tr("Extraction completed"));
}

void TarBackend::compress(const CompressOptions &options)
{
    const QString tarPath = ProcessHelper::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    if (!options.password.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
            tr("tar does not support encryption; use 7z or zip instead"));
        return;
    }

    // Validate compression level
    int level = validateCompressionLevel(options.compressionLevel);

    QStringList args;
    args << QStringLiteral("-c") << QStringLiteral("-v");

    // Compression flag + level via --use-compress-program
    QString flag = compressFlag(m_format);
    if (m_format == ArchiveFormat::Tar) {
        // No compression
    } else if (level > 0) {
        QString prog;
        switch (m_format) {
        case ArchiveFormat::TarGz:  prog = QStringLiteral("gzip -%1").arg(level); break;
        case ArchiveFormat::TarBz2: prog = QStringLiteral("bzip2 -%1").arg(level); break;
        case ArchiveFormat::TarXz:  prog = QStringLiteral("xz -T0 -%1").arg(level); break;
        default: break;
        }
        args << QStringLiteral("--use-compress-program") << prog;
    } else {
        // Use default flag
        args << flag;
    }

    args << excludeArgs(options.excludePatterns);

    // Determine base dir for relative paths
    QString baseDir;
    QStringList relFiles;
    if (options.sourceFiles.size() == 1) {
        QFileInfo fi(options.sourceFiles.first());
        baseDir = fi.absolutePath();
        relFiles << fi.fileName();
    } else {
        // Use common parent
        QDir d(options.sourceFiles.first());
        baseDir = d.absolutePath();
        for (const QString &f : options.sourceFiles) {
            QFileInfo fi(f);
            if (fi.absolutePath() == baseDir) {
                relFiles << fi.fileName();
            } else {
                relFiles << f;
                baseDir = QFileInfo(f).absolutePath();
            }
        }
    }

    args << QStringLiteral("-f") << options.archivePath
         << QStringLiteral("-C") << baseDir;
    for (const QString &f : relFiles) {
        args << f;
    }

    LOG_DEBUG("TarBackend", "Compress: %s", ProcessHelper::maskedCommand(tarPath, args).toUtf8().constData());

    QProcess proc;
    proc.start(tarPath, args);
    if (!proc.waitForStarted(5000)) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start tar"));
        return;
    }

    int processed = 0;
    const int total = FileUtils::countFiles(options.sourceFiles);
    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(30000);
        QByteArray data = proc.readAllStandardOutput();
        if (!data.isEmpty()) {
            emit outputReady(QString::fromLocal8Bit(data).trimmed());
            ++processed;
            int percent = total > 0 ? (processed * 100 / total) : 0;
            emit progressChanged(percent, total, processed, -1);
        }
        if (proc.state() == QProcess::NotRunning) break;
    }

    if (proc.exitCode() != 0) {
        QFile::remove(options.archivePath);
        emit errorOccurred(ArchiveErrorCode::Unknown,
            tr("Compression failed: %1").arg(QString::fromLocal8Bit(proc.readAllStandardError())));
        return;
    }
    emit operationFinished(true, tr("Archive created: %1").arg(options.archivePath));
}

void TarBackend::addFiles(const AddFilesOptions &options)
{
    if (!supportsAppend()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
            tr("Cannot add files to compressed tar archives"));
        return;
    }

    const QString tarPath = ProcessHelper::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("-r") << QStringLiteral("-v")
         << QStringLiteral("-f") << options.archivePath;

    for (const QString &f : options.sourceFiles) {
        args << f;
    }

    QProcess proc;
    proc.start(tarPath, args);
    proc.waitForFinished(60000);

    if (proc.exitCode() != 0) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
            tr("Add files failed: %1").arg(QString::fromLocal8Bit(proc.readAllStandardError())));
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

    const QString tarPath = ProcessHelper::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("--delete") << QStringLiteral("-v")
         << QStringLiteral("-f") << options.archivePath;
    for (const QString &e : options.targetEntries) {
        args << FileUtils::normalizeEntryPath(e);
    }

    QProcess proc;
    proc.start(tarPath, args);
    proc.waitForFinished(60000);

    if (proc.exitCode() != 0) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
            tr("Delete files failed: %1").arg(QString::fromLocal8Bit(proc.readAllStandardError())));
        return;
    }
    emit operationFinished(true, tr("Files deleted from archive"));
}

void TarBackend::testArchive(const TestOptions &options)
{
    Q_UNUSED(options);
    const QString tarPath = ProcessHelper::resolveToolPath(QStringLiteral("tar"));
    if (tarPath.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("tar tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("-t") << decompressFlag()
         << QStringLiteral("-f") << m_archivePath;

    QProcess proc;
    proc.setStandardOutputFile(QProcess::nullDevice());
    proc.start(tarPath, args);
    proc.waitForFinished(60000);

    if (proc.exitCode() != 0) {
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
    // Only uncompressed .tar supports add/delete
    return m_format == ArchiveFormat::Tar;
}
