#include "RarBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <QDirIterator>
#include <QRegularExpression>

#include "utils/Logger.h"
#include "utils/FileUtils.h"
#include "utils/ProcessHelper.h"
#include "utils/MimeTypeHelper.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveError.h"

RarBackend::RarBackend(ArchiveFormat fmt, QObject *parent)
    : ArchiveBackend(parent)
    , m_format(fmt)
{
    m_process = new ProcessHelper(this);
    connect(m_process, &ProcessHelper::outputLine, this, &ArchiveBackend::outputReady);
}

QString RarBackend::unrarPath() const
{
    return ProcessHelper::resolveToolPath(QStringLiteral("unrar"));
}

QString RarBackend::resolveFirstVolume() const
{
    return MimeTypeHelper::resolveFirstVolume(m_archivePath);
}

QList<ArchiveEntry> RarBackend::parseList(const QString &output) const
{
    QList<ArchiveEntry> entries;
    QString data = output;
    QTextStream stream(&data);
    stream.setEncoding(QStringConverter::Utf8);
    QString line;
    bool inEntries = false;

    while (stream.readLineInto(&line)) {
        // unrar lt (technical) output has a header then "----" then entries
        if (line.startsWith(QStringLiteral("----"))) {
            inEntries = !inEntries;
            continue;
        }
        if (!inEntries) {
            // Detect encryption markers in header
            if (line.contains(QStringLiteral("*"), Qt::CaseSensitive) ||
                line.contains(QStringLiteral("Encrypted"), Qt::CaseInsensitive)) {
                const_cast<RarBackend*>(this)->m_encrypted = true;
            }
            continue;
        }

        // Parse entry line: "perms size packed ratio date time attrs name"
        // Simplified parse: split on whitespace
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 7) continue;

        ArchiveEntry e;
        // Find the date (YYYY-MM-DD) to locate fields reliably
        int dateIdx = -1;
        static const QRegularExpression dateRe(QStringLiteral("\\d{4}-\\d{2}-\\d{2}"));
        for (int i = 0; i < parts.size(); ++i) {
            if (dateRe.match(parts.at(i)).hasMatch()) {
                dateIdx = i;
                break;
            }
        }
        if (dateIdx < 0) continue;

        // size is typically at index 1 (after perms)
        bool ok = false;
        e.size = parts.at(1).toLongLong(&ok);
        if (!ok) e.size = parts.value(2).toLongLong();

        // Date and time
        if (dateIdx + 1 < parts.size()) {
            e.mtime = QDateTime::fromString(parts.at(dateIdx) + ' ' + parts.at(dateIdx + 1),
                                            QStringLiteral("yyyy-MM-dd HH:MM"));
            if (!e.mtime.isValid()) {
                e.mtime = QDateTime::fromString(parts.at(dateIdx) + ' ' + parts.at(dateIdx + 1),
                                                QStringLiteral("yyyy-MM-dd hh:mm"));
            }
        }

        // Name: everything after time field
        if (dateIdx + 2 < parts.size()) {
            e.path = parts.mid(dateIdx + 2).join(' ');
        }

        // Permissions to detect dir/symlink
        if (!parts.isEmpty()) {
            const QString &perms = parts.at(0);
            e.isDir = perms.startsWith('d');
            e.isSymLink = perms.startsWith('l');
        }

        if (m_encrypted) {
            e.isEncrypted = true;
        }

        if (!e.path.isEmpty()) {
            entries.append(e);
        }
    }
    return entries;
}

void RarBackend::openArchive(const QString &filePath)
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

void RarBackend::listContents(const ListOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    const QString unrar = unrarPath();
    if (unrar.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("unrar tool not found"));
        return;
    }

    const QString firstVol = resolveFirstVolume();

    QStringList args;
    args << QStringLiteral("lt") << firstVol;
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }

    QProcess proc;
    proc.start(unrar, args);
    proc.waitForFinished(30000);

    if (proc.exitCode() != 0) {
        QString err = QString::fromLocal8Bit(proc.readAllStandardError());
        if (err.contains(QStringLiteral("password"), Qt::CaseInsensitive) ||
            proc.exitCode() == 2) {
            m_encrypted = true;
            if (options.password.isEmpty()) {
                emit passwordRequested();
                return;
            }
            emit errorOccurred(ArchiveErrorCode::WrongPassword, tr("Wrong password"));
        } else {
            emit errorOccurred(ArchiveErrorCode::Unknown, tr("List failed: %1").arg(err));
        }
        return;
    }

    QString output = QString::fromLocal8Bit(proc.readAllStandardOutput());
    m_entries = parseList(output);
    emit fileListReady(m_entries);
}

void RarBackend::extract(const ExtractOptions &options)
{
    if (!m_isOpen) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("Archive not opened"));
        return;
    }

    const QString unrar = unrarPath();
    if (unrar.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("unrar tool not found"));
        return;
    }

    QDir().mkpath(options.destDir);
    const QString tempDir = FileUtils::createTempExtractDir(options.destDir);
    const QString firstVol = resolveFirstVolume();

    QStringList args;
    args << QStringLiteral("x") << firstVol << tempDir + QDir::separator() << QStringLiteral("-y");
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }

    for (const QString &sel : options.selectedFiles) {
        args << FileUtils::normalizeEntryPath(sel);
    }

    LOG_DEBUG("RarBackend", "Extract: %s", ProcessHelper::maskedCommand(unrar, args).toUtf8().constData());

    QProcess proc;
    proc.start(unrar, args);
    proc.waitForStarted(5000);

    int processed = 0;
    const int total = options.selectedFiles.isEmpty() ? m_entries.size() : options.selectedFiles.size();

    while (proc.state() != QProcess::NotRunning) {
        proc.waitForReadyRead(30000);
        QByteArray data = proc.readAllStandardOutput();
        if (!data.isEmpty()) {
            QString text = QString::fromLocal8Bit(data);
            emit outputReady(text.trimmed());
            for (const QString &l : text.split('\n')) {
                if (l.contains(QStringLiteral("Extracting"), Qt::CaseInsensitive) ||
                    l.contains(QStringLiteral("OK"))) {
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
        if (proc.exitCode() == 2 || proc.exitCode() == 3) {
            emit errorOccurred(ArchiveErrorCode::WrongPassword,
                tr("Extraction failed (wrong password or corrupted)"));
        } else {
            emit errorOccurred(ArchiveErrorCode::Unknown,
                tr("Extraction failed (exit code %1)").arg(proc.exitCode()));
        }
        return;
    }

    // Move everything from the temp dir into destDir (directories recreated,
    // files/symlinks renamed).
    const int failedMoves = FileUtils::moveExtractedContents(tempDir, options.destDir,
                                                             options.overwrite);
    if (failedMoves > 0) {
        LOG_WARN("RarBackend", "Failed to move %d extracted entr%s",
                 failedMoves, failedMoves == 1 ? "y" : "ies");
    }

    emit operationFinished(true, tr("Extraction completed"));
}

void RarBackend::compress(const CompressOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
        tr("RAR compression is not supported (RARLAB license)"));
}

void RarBackend::addFiles(const AddFilesOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
        tr("RAR does not support adding files (read-only)"));
}

void RarBackend::deleteFiles(const DeleteFilesOptions &)
{
    emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
        tr("RAR does not support deleting files (read-only)"));
}

void RarBackend::testArchive(const TestOptions &options)
{
    const QString unrar = unrarPath();
    if (unrar.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("unrar tool not found"));
        return;
    }

    const QString firstVol = resolveFirstVolume();
    QStringList args;
    args << QStringLiteral("t") << firstVol;
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }

    QProcess proc;
    proc.start(unrar, args);
    proc.waitForFinished(120000);

    if (proc.exitCode() != 0) {
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive, tr("Archive test failed"));
        return;
    }
    emit operationFinished(true, tr("Archive integrity verified"));
}

void RarBackend::closeArchive()
{
    m_isOpen = false;
    m_archivePath.clear();
    m_entries.clear();
    m_encrypted = false;
}
