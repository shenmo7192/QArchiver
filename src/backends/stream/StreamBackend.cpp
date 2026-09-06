#include "StreamBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QDirIterator>

#include "utils/Logger.h"
#include "utils/FileUtils.h"
#include "utils/ProcessHelper.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveError.h"

StreamBackend::StreamBackend(ArchiveFormat fmt, QObject *parent)
    : ArchiveBackend(parent)
    , m_format(fmt)
{
    m_process = new ProcessHelper(this);
    connect(m_process, &ProcessHelper::outputLine, this, &ArchiveBackend::outputReady);
}

QString StreamBackend::sevenZipPath() const
{
    return ProcessHelper::resolveToolPath(QStringLiteral("7z"));
}

QString StreamBackend::inferInternalName() const
{
    QFileInfo fi(m_archivePath);
    QString baseName = fi.fileName();
    // Strip the compression suffix
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

void StreamBackend::openArchive(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        emit errorOccurred(ArchiveErrorCode::FileNotFound, tr("File not found: %1").arg(filePath));
        return;
    }
    m_archivePath = filePath;
    m_isOpen = true;
    m_entry.path = inferInternalName();
    m_entry.size = fi.size();
    emit archiveOpened(filePath);
}

void StreamBackend::listContents(const ListOptions &)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    // Pure streams have a single entry; infer the name
    m_entry.path = inferInternalName();
    emit fileListReady({m_entry});
}

void StreamBackend::extract(const ExtractOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    const QString sz = sevenZipPath();
    if (sz.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    QDir().mkpath(options.destDir);
    const QString tempDir = FileUtils::createTempExtractDir(options.destDir);

    QStringList args;
    args << QStringLiteral("x") << m_archivePath
         << QStringLiteral("-o") + tempDir << QStringLiteral("-y");

    LOG_DEBUG("StreamBackend", "Extract: %s", ProcessHelper::maskedCommand(sz, args).toUtf8().constData());

    QProcess proc;
    proc.start(sz, args);
    proc.waitForStarted(5000);

    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(30000);
        QByteArray data = proc.readAllStandardOutput();
        if (!data.isEmpty()) {
            emit outputReady(QString::fromUtf8(data).trimmed());
            emit progressChanged(50, 1, 0, -1);
        }
        if (proc.state() == QProcess::NotRunning) break;
    }

    if (proc.exitCode() != 0) {
        FileUtils::removeDirectoryRecursively(tempDir);
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
            tr("Decompression failed (corrupted stream)"));
        return;
    }

    // Rename extracted file to inferred name and move
    const QString inferredName = inferInternalName();
    QDir tempDirObj(tempDir);
    QDirIterator it(tempDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString destPath = QDir(options.destDir).filePath(inferredName);
        if (QFileInfo::exists(destPath)) {
            if (options.overwrite == OverwritePolicy::Never) continue;
            QFile::remove(destPath);
        }
        FileUtils::moveFileAtomically(it.filePath(), destPath);
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
    const QString sz = sevenZipPath();
    if (sz.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    QStringList args;
    args << QStringLiteral("t") << m_archivePath;

    QProcess proc;
    proc.start(sz, args);
    proc.waitForFinished(60000);

    if (proc.exitCode() != 0) {
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive, tr("Stream test failed"));
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
