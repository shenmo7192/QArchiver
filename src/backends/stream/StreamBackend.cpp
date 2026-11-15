#include "StreamBackend.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include "utils/FileUtils.h"
#include "utils/Logger.h"
#include "utils/ProcessRunner.h"

StreamBackend::StreamBackend(ArchiveFormat fmt, QObject *parent)
    : ArchiveBackend(parent)
    , m_format(fmt)
{
}

QString StreamBackend::sevenZipPath() const
{
    return ProcessRunner::resolveToolPath(QStringLiteral("7z"));
}

QString StreamBackend::inferInternalName() const
{
    QFileInfo info(m_archivePath);
    QString baseName = info.fileName();
    if (baseName.endsWith(QStringLiteral(".gz"), Qt::CaseInsensitive)) {
        baseName.chop(3);
    } else if (baseName.endsWith(QStringLiteral(".bz2"), Qt::CaseInsensitive)) {
        baseName.chop(4);
    } else if (baseName.endsWith(QStringLiteral(".xz"), Qt::CaseInsensitive)) {
        baseName.chop(3);
    }
    if (baseName.isEmpty()) {
        baseName = QStringLiteral("output");
    }
    return baseName;
}

qint64 StreamBackend::queryUncompressedSize() const
{
    const QString sevenZip = sevenZipPath();
    if (sevenZip.isEmpty()) {
        return -1;
    }

    const ProcessResult result = ProcessRunner::run(
        sevenZip,
        {QStringLiteral("l"), m_archivePath, QStringLiteral("-slt")},
        QStringLiteral("UTF-8"), QProcessEnvironment(), QString(), 30000);

    if (!result.success()) {
        return -1;
    }

    // The entry block contains "Size = <uncompressed>" and "Packed Size =".
    static const QRegularExpression sizeRe(
        QStringLiteral("^Size\\s*=\\s*(\\d+)$"));
    static const QRegularExpression packedRe(
        QStringLiteral("^Packed Size\\s*=\\s*(\\d+)$"));

    qint64 size = -1;
    const QStringList lines = result.stdOut.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const auto packed = packedRe.match(line.trimmed());
        if (packed.hasMatch()) {
            // We are inside an entry block; the preceding "Size" is the
            // uncompressed size.
            break;
        }
        const auto match = sizeRe.match(line.trimmed());
        if (match.hasMatch()) {
            size = match.captured(1).toLongLong();
        }
    }
    return size;
}

void StreamBackend::openArchive(const QString &filePath)
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
    m_entry.path = inferInternalName();
    m_entry.size = info.size();
    emit archiveOpened(filePath);
}

void StreamBackend::listContents(const ListOptions &)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    m_entry.path = inferInternalName();
    const qint64 size = queryUncompressedSize();
    if (size >= 0) {
        m_entry.size = size;
    }
    emit fileListReady({m_entry});
}

void StreamBackend::extract(const ExtractOptions &options)
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

    QDir().mkpath(options.destDir);
    const QString tempDir = FileUtils::createTempExtractDir(options.destDir);

    QStringList args;
    args << QStringLiteral("x") << m_archivePath
         << QStringLiteral("-o") + tempDir
         << QStringLiteral("-y");

    LOG_DEBUG("StreamBackend", "Extract: %s",
              ProcessRunner::maskedCommand(sevenZip, args).toUtf8().constData());

    const ProcessResult result = ProcessRunner::run(
        sevenZip, args, QStringLiteral("UTF-8"), QProcessEnvironment(),
        QString(), 0, &m_cancelRequested,
        [this](const QString &line, bool isError) {
            if (!isError && !line.isEmpty()) {
                emit outputReady(line);
                emit progressChanged(50, 1, 0, -1);
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
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                           tr("Decompression failed (corrupted stream)"));
        return;
    }

    QStringList extractedFiles;
    QDirIterator it(tempDir, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        extractedFiles << it.next();
    }

    if (extractedFiles.isEmpty()) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                           tr("The stream did not contain any file"));
        return;
    }

    const QString inferredName = inferInternalName();
    int index = 0;
    for (const QString &source : extractedFiles) {
        QString destName = (index == 0 && !inferredName.isEmpty())
            ? inferredName
            : QFileInfo(source).fileName();
        ++index;

        QString destPath = QDir(options.destDir).filePath(destName);
        if (QFileInfo::exists(destPath) || QFileInfo(destPath).isSymLink()) {
            switch (options.overwrite) {
            case OverwritePolicy::Never:
                continue;
            case OverwritePolicy::Rename:
                destPath = FileUtils::uniqueDestinationPath(destPath);
                break;
            case OverwritePolicy::Ask:
            case OverwritePolicy::Always:
                QFile::remove(destPath);
                break;
            }
        }

        if (!FileUtils::moveFileAtomically(source, destPath)) {
            FileUtils::removeDirectoryRecursively(tempDir);
            emit errorOccurred(ArchiveErrorCode::Unknown,
                               tr("Failed to move extracted file to %1").arg(destPath));
            return;
        }
    }
    FileUtils::removeDirectoryRecursively(tempDir);

    emit progressChanged(100, 1, 1, -1);
    emit operationFinished(true, tr("Decompression completed"));
}

void StreamBackend::compress(const CompressOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Creating pure compression streams is not supported"));
}

void StreamBackend::addFiles(const AddFilesOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Cannot add files to a pure compression stream"));
}

void StreamBackend::deleteFiles(const DeleteFilesOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                       tr("Cannot delete files from a pure compression stream"));
}

void StreamBackend::testArchive(const TestOptions &)
{
    const QString sevenZip = sevenZipPath();
    if (sevenZip.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    const ProcessResult result = ProcessRunner::run(
        sevenZip,
        {QStringLiteral("t"), m_archivePath},
        QStringLiteral("UTF-8"), QProcessEnvironment(), QString(),
        300000, &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.success()) {
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                           tr("Stream test failed"));
        return;
    }
    emit operationFinished(true, tr("Stream integrity verified"));
}

void StreamBackend::closeArchive()
{
    m_isOpen = false;
    m_archivePath.clear();
    m_entry = ArchiveEntry();
}
