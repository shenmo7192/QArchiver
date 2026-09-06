#include "SevenZipBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDirIterator>
#include <algorithm>

#include "utils/Logger.h"
#include "utils/FileUtils.h"
#include "utils/ProcessHelper.h"
#include "utils/MimeTypeHelper.h"
#include "core/ArchiveOptions.h"
#include "core/AppSettings.h"

SevenZipBackend::SevenZipBackend(QObject *parent)
    : ArchiveBackend(parent)
{
    m_process = new ProcessHelper(this);
    connect(m_process, &ProcessHelper::outputLine, this, &ArchiveBackend::outputReady);
}

QString SevenZipBackend::sevenZipPath() const
{
    return ProcessHelper::resolveToolPath(QStringLiteral("7z"));
}

bool SevenZipBackend::isSplitArchive() const
{
    return MimeTypeHelper::isSplitVolume(m_archivePath);
}

QList<ArchiveEntry> SevenZipBackend::parseList(const QString &output) const
{
    // Parse 7z `l -slt` (technical) output. The archive header block and each
    // entry are "Key = Value" line groups; a new entry starts with "Path =".
    // Only blocks that contain an entry-specific line are actual archive
    // entries. For .zip the marker is "Folder = +/-", but for .7z archives
    // 7-Zip 23.x omits the "Folder" line and expresses directories through
    // "Attributes = D ...". The header block has "Path = <archive>" but
    // neither "Folder" nor "Attributes".
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
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0) {
            continue;  // Separator lines like "--" or "----------"
        }
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        if (key == QLatin1String("Path")) {
            commitEntry();
            current.path = value;
        } else if (key == QLatin1String("Folder")) {
            current.isDir = (value == QLatin1String("+"));
            currentIsEntry = true;
        } else if (key == QLatin1String("Attributes")) {
            // .7z entries have no "Folder" line in 7-Zip 23.x; the directory
            // flag is the leading 'D' in "Attributes = D drwx...".
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

int SevenZipBackend::validateLevel(int level) const
{
    if (level < 0 || level > 9) {
        return AppSettings::instance().defaultCompressionLevel();
    }
    return level;
}

void SevenZipBackend::openArchive(const QString &filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        emit errorOccurred(ArchiveErrorCode::FileNotFound, tr("File not found: %1").arg(filePath));
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

    const QString sz = sevenZipPath();
    if (sz.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    QString target = m_archivePath;
    if (isSplitArchive()) {
        target = MimeTypeHelper::resolveFirstVolume(m_archivePath);
    }

    QStringList args;
    args << QStringLiteral("l") << target << QStringLiteral("-slt");
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }

    QProcess proc;
    proc.start(sz, args);
    proc.waitForFinished(30000);

    if (proc.exitCode() != 0) {
        QString err = QString::fromUtf8(proc.readAllStandardError());
        if (err.contains(QStringLiteral("password"), Qt::CaseInsensitive) ||
            err.contains(QStringLiteral("Can not open"), Qt::CaseInsensitive)) {
            m_encrypted = true;
            if (options.password.isEmpty()) {
                emit passwordRequested();
                return;
            }
            emit errorOccurred(ArchiveErrorCode::WrongPassword, tr("Wrong password"));
            return;
        }
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("List failed: %1").arg(err));
        return;
    }

    QString output = QString::fromUtf8(proc.readAllStandardOutput());
    m_entries = parseList(output);

    // Entries marked with "Encrypted = +" carry encryption info.
    m_encrypted = std::any_of(m_entries.cbegin(), m_entries.cend(),
                              [](const ArchiveEntry &e) { return e.isEncrypted; });
    emit fileListReady(m_entries);
}

void SevenZipBackend::extract(const ExtractOptions &options)
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

    QString target = m_archivePath;
    if (isSplitArchive()) {
        target = MimeTypeHelper::resolveFirstVolume(m_archivePath);
    }

    QDir().mkpath(options.destDir);
    const QString tempDir = FileUtils::createTempExtractDir(options.destDir);

    QStringList args;
    args << QStringLiteral("x") << target << QStringLiteral("-o") + tempDir;

    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }
    args << QStringLiteral("-y");

    for (const QString &sel : options.selectedFiles) {
        args << FileUtils::normalizeEntryPath(sel);
    }

    LOG_DEBUG("SevenZipBackend", "Extract: %s", ProcessHelper::maskedCommand(sz, args).toUtf8().constData());

    QProcess proc;
    proc.start(sz, args);
    proc.waitForStarted(5000);

    int processed = 0;
    const int total = options.selectedFiles.isEmpty() ? m_entries.size() : options.selectedFiles.size();

    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(30000);
        QByteArray data = proc.readAllStandardOutput();
        if (!data.isEmpty()) {
            QString text = QString::fromUtf8(data);
            emit outputReady(text.trimmed());
            for (const QString &l : text.split('\n')) {
                if (l.startsWith(QStringLiteral("U -")) || l.startsWith(QStringLiteral("+ "))) {
                    ++processed;
                }
            }
            int percent = total > 0 ? (processed * 100 / total) : 0;
            emit progressChanged(percent, total, processed, -1);
        }
        if (proc.state() == QProcess::NotRunning) break;
    }

    if (proc.exitCode() != 0) {
        FileUtils::removeDirectoryRecursively(tempDir);
        QString err = QString::fromUtf8(proc.readAllStandardError());
        if (proc.exitCode() == 2 && m_encrypted) {
            emit errorOccurred(ArchiveErrorCode::WrongPassword, tr("Wrong password"));
        } else {
            emit errorOccurred(ArchiveErrorCode::Unknown, tr("Extraction failed: %1").arg(err));
        }
        return;
    }

    // Move everything from the temp dir into destDir (directories recreated,
    // files/symlinks renamed).
    const int failedMoves = FileUtils::moveExtractedContents(tempDir, options.destDir,
                                                             options.overwrite);
    if (failedMoves > 0) {
        LOG_WARN("SevenZipBackend", "Failed to move %d extracted entr%s",
                 failedMoves, failedMoves == 1 ? "y" : "ies");
    }

    emit operationFinished(true, tr("Extraction completed"));
}

void SevenZipBackend::compress(const CompressOptions &options)
{
    const QString sz = sevenZipPath();
    if (sz.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("7z tool not found"));
        return;
    }

    int level = validateLevel(options.compressionLevel);

    QStringList args;
    args << QStringLiteral("a") << options.archivePath
         << QStringLiteral("-mx=%1").arg(level) << QStringLiteral("-mmt=on");

    if (options.solidMode) {
        args << QStringLiteral("-ms=on");
    }
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
        if (options.encryptHeaders) {
            args << QStringLiteral("-mhe=on");
        }
    }
    if (options.volumeSize > 0) {
        args << QStringLiteral("-v%1b").arg(options.volumeSize);
    }

    for (const QString &f : options.sourceFiles) {
        args << f;
    }

    LOG_DEBUG("SevenZipBackend", "Compress: %s", ProcessHelper::maskedCommand(sz, args).toUtf8().constData());

    QProcess proc;
    proc.start(sz, args);
    proc.waitForStarted(5000);

    int processed = 0;
    const int total = FileUtils::countFiles(options.sourceFiles);
    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(30000);
        QByteArray data = proc.readAllStandardOutput();
        if (!data.isEmpty()) {
            QString text = QString::fromUtf8(data);
            emit outputReady(text.trimmed());
            for (const QString &l : text.split('\n')) {
                if (l.startsWith(QStringLiteral("+ "))) ++processed;
            }
            int percent = total > 0 ? (processed * 100 / total) : 0;
            emit progressChanged(percent, total, processed, -1);
        }
        if (proc.state() == QProcess::NotRunning) break;
    }

    if (proc.exitCode() != 0) {
        QFile::remove(options.archivePath);
        emit errorOccurred(ArchiveErrorCode::Unknown,
            tr("Compression failed: %1").arg(QString::fromUtf8(proc.readAllStandardError())));
        return;
    }
    emit operationFinished(true, tr("Archive created: %1").arg(options.archivePath));
}

void SevenZipBackend::addFiles(const AddFilesOptions &options)
{
    if (!supportsAppend()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
            tr("Cannot add files to a split archive"));
        return;
    }

    const QString sz = sevenZipPath();
    QStringList args;
    args << QStringLiteral("a") << options.archivePath;
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }
    for (const QString &f : options.sourceFiles) {
        args << f;
    }

    QProcess proc;
    proc.start(sz, args);
    proc.waitForFinished(120000);

    if (proc.exitCode() != 0) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
            tr("Add files failed: %1").arg(QString::fromUtf8(proc.readAllStandardError())));
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

    const QString sz = sevenZipPath();
    QStringList args;
    args << QStringLiteral("d") << options.archivePath;
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }
    for (const QString &e : options.targetEntries) {
        args << FileUtils::normalizeEntryPath(e);
    }

    QProcess proc;
    proc.start(sz, args);
    proc.waitForFinished(120000);

    if (proc.exitCode() != 0) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
            tr("Delete files failed: %1").arg(QString::fromUtf8(proc.readAllStandardError())));
        return;
    }
    emit operationFinished(true, tr("Files deleted from archive"));
}

void SevenZipBackend::testArchive(const TestOptions &options)
{
    const QString sz = sevenZipPath();
    QStringList args;
    args << QStringLiteral("t") << m_archivePath;
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }

    QProcess proc;
    proc.start(sz, args);
    proc.waitForFinished(120000);

    if (proc.exitCode() != 0) {
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive, tr("Archive test failed"));
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
