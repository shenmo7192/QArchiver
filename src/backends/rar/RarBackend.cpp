#include "RarBackend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTextStream>

#include "utils/FileUtils.h"
#include "utils/Logger.h"
#include "utils/MimeTypeHelper.h"
#include "utils/ProcessRunner.h"

namespace {

QProcessEnvironment unrarEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    env.insert(QStringLiteral("LANG"), QStringLiteral("C"));
    return env;
}

} // namespace

RarBackend::RarBackend(ArchiveFormat fmt, QObject *parent)
    : ArchiveBackend(parent)
    , m_format(fmt)
{
}

QString RarBackend::unrarPath() const
{
    return ProcessRunner::resolveToolPath(QStringLiteral("unrar"));
}

QString RarBackend::resolveFirstVolume() const
{
    return MimeTypeHelper::resolveFirstVolume(m_archivePath);
}

QList<ArchiveEntry> RarBackend::parseList(const QString &output)
{
    QList<ArchiveEntry> entries;
    const QStringList lines = output.split(QLatin1Char('\n'));

    // `unrar lt` (technical listing) is a sequence of "Key: Value" blocks.
    // Older unrar builds/tables use the `----` separated table form, which is
    // handled by the fallback below.
    bool technical = false;
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(QStringLiteral("Name:"))) {
            technical = true;
            break;
        }
    }

    if (technical) {
        ArchiveEntry current;
        bool hasEntry = false;
        auto commit = [&]() {
            if (hasEntry && !current.path.isEmpty()) {
                current.isEncrypted = m_encrypted;
                entries.append(current);
            }
            current = ArchiveEntry{};
            hasEntry = false;
        };

        for (const QString &rawLine : lines) {
            const QString line = rawLine.trimmed();
            if (line.startsWith(QStringLiteral("Name:"))) {
                commit();
                current.path = line.mid(5).trimmed();
                hasEntry = true;
            } else if (!hasEntry) {
                if (line.contains(QStringLiteral("Encrypted"), Qt::CaseInsensitive)) {
                    m_encrypted = true;
                }
                continue;
            } else if (line.startsWith(QStringLiteral("Type:"))) {
                current.isDir = line.mid(5).trimmed().compare(
                    QStringLiteral("Directory"), Qt::CaseInsensitive) == 0;
            } else if (line.startsWith(QStringLiteral("Packed size:"))) {
                current.packedSize = line.mid(12).trimmed().toLongLong();
            } else if (line.startsWith(QStringLiteral("Size:"))) {
                current.size = line.mid(5).trimmed().toLongLong();
            } else if (line.startsWith(QStringLiteral("Modified:"))) {
                const QString value = line.mid(9).trimmed();
                current.mtime = QDateTime::fromString(
                    value.left(19), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
                if (!current.mtime.isValid()) {
                    current.mtime = QDateTime::fromString(
                        value.left(16), QStringLiteral("yyyy-MM-dd HH:mm"));
                }
            } else if (line.startsWith(QStringLiteral("Attributes:"))) {
                const QString value = line.mid(11).trimmed();
                current.isDir = current.isDir || value.startsWith(QLatin1Char('d'));
                current.isSymLink = value.startsWith(QLatin1Char('l'));
            } else if (line.startsWith(QStringLiteral("Flags:"))
                       || line.startsWith(QStringLiteral("Encrypted:"))) {
                if (line.contains(QStringLiteral("encrypt"), Qt::CaseInsensitive)) {
                    m_encrypted = true;
                }
            }
        }
        commit();
        return entries;
    }

    QString data = output;
    QTextStream stream(&data);
    stream.setEncoding(QStringConverter::Utf8);
    QString line;
    bool inEntries = false;

    while (stream.readLineInto(&line)) {
        if (line.startsWith(QStringLiteral("----"))) {
            inEntries = !inEntries;
            continue;
        }
        if (!inEntries) {
            if (line.contains(QLatin1Char('*'))
                || line.contains(QStringLiteral("Encrypted"), Qt::CaseInsensitive)) {
                m_encrypted = true;
            }
            continue;
        }

        const QStringList parts = line.split(
            QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        if (parts.size() < 7) {
            continue;
        }

        ArchiveEntry entry;

        static const QRegularExpression dateRe(QStringLiteral("\\d{4}-\\d{2}-\\d{2}"));
        int dateIndex = -1;
        for (int i = 0; i < parts.size(); ++i) {
            if (dateRe.match(parts.at(i)).hasMatch()) {
                dateIndex = i;
                break;
            }
        }
        if (dateIndex < 0) {
            continue;
        }

        bool ok = false;
        entry.size = parts.at(1).toLongLong(&ok);
        if (!ok && parts.size() > 2) {
            entry.size = parts.at(2).toLongLong();
        }

        if (dateIndex + 1 < parts.size()) {
            entry.mtime = QDateTime::fromString(
                parts.at(dateIndex) + QLatin1Char(' ') + parts.at(dateIndex + 1),
                QStringLiteral("yyyy-MM-dd HH:mm"));
        }

        if (dateIndex + 2 < parts.size()) {
            // Keep the path from the original line instead of joining split
            // tokens: this preserves repeated spaces in file names.
            const QString needle = parts.at(dateIndex) + QLatin1Char(' ')
                + parts.at(dateIndex + 1);
            const int pos = line.indexOf(needle);
            entry.path = pos >= 0 ? line.mid(pos + needle.size()).trimmed()
                                  : parts.mid(dateIndex + 2).join(QLatin1Char(' '));
        }

        const QString &permissions = parts.at(0);
        entry.isDir = permissions.startsWith(QLatin1Char('d'));
        entry.isSymLink = permissions.startsWith(QLatin1Char('l'));
        entry.isEncrypted = m_encrypted;

        if (!entry.path.isEmpty()) {
            entries.append(entry);
        }
    }
    return entries;
}

void RarBackend::openArchive(const QString &filePath)
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

    QStringList args;
    args << QStringLiteral("lt") << QStringLiteral("-scu")
         << resolveFirstVolume();
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }

    const ProcessResult result = ProcessRunner::run(
        unrar, args, options.encodingHint, unrarEnvironment(), QString(),
        300000, &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.started) {
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start unrar"));
        return;
    }

    if (!result.success()) {
        const QString errorText = result.stdErr;
        const bool passwordRelated = errorText.contains(QStringLiteral("password"), Qt::CaseInsensitive)
            || result.exitCode == 2 || result.exitCode == 3;

        if (passwordRelated) {
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

    for (const QString &selected : options.selectedFiles) {
        if (FileUtils::isPathTraversal(selected, options.destDir)) {
            emit errorOccurred(ArchiveErrorCode::PathTraversalBlocked,
                               tr("Blocked unsafe path: %1").arg(selected));
            return;
        }
    }

    QDir().mkpath(options.destDir);
    const QString tempDir = FileUtils::createTempExtractDir(options.destDir);

    // unrar syntax: x [options] archive [files...] [destination]
    QStringList args;
    args << QStringLiteral("x") << QStringLiteral("-y") << QStringLiteral("-scu");
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }
    args << resolveFirstVolume();

    for (const QString &selected : options.selectedFiles) {
        const QString normalized = FileUtils::normalizeEntryPath(selected);
        if (!normalized.isEmpty()) {
            // unrar matches the member names exactly as listed.
            args << selected;
        }
    }
    args << tempDir + QDir::separator();

    LOG_DEBUG("RarBackend", "Extract: %s",
              ProcessRunner::maskedCommand(unrar, args).toUtf8().constData());

    const int total = options.selectedFiles.isEmpty()
        ? m_entries.size()
        : options.selectedFiles.size();
    int processed = 0;

    const ProcessResult result = ProcessRunner::run(
        unrar, args, options.encodingHint, unrarEnvironment(), QString(),
        0, &m_cancelRequested,
        [this, total, &processed](const QString &line, bool isError) {
            if (isError || line.isEmpty()) {
                return;
            }
            emit outputReady(line);
            if (line.contains(QStringLiteral("Extracting"), Qt::CaseInsensitive)) {
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
        emit errorOccurred(ArchiveErrorCode::ToolNotFound, tr("Failed to start unrar"));
        return;
    }
    if (!result.success()) {
        FileUtils::removeDirectoryRecursively(tempDir);
        if (result.exitCode == 2 || result.exitCode == 3) {
            emit errorOccurred(ArchiveErrorCode::WrongPassword,
                               tr("Extraction failed (wrong password or corrupted)"));
        } else {
            emit errorOccurred(ArchiveErrorCode::Unknown,
                               tr("Extraction failed (exit code %1)").arg(result.exitCode));
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

    QStringList args;
    args << QStringLiteral("t") << QStringLiteral("-scu");
    if (!options.password.isEmpty()) {
        args << QStringLiteral("-p") + options.password;
    }
    args << resolveFirstVolume();

    const ProcessResult result = ProcessRunner::run(
        unrar, args, QString(), unrarEnvironment(), QString(),
        300000, &m_cancelRequested);

    if (result.cancelled) {
        emit errorOccurred(ArchiveErrorCode::Cancelled, tr("Operation cancelled"));
        return;
    }
    if (!result.success()) {
        emit errorOccurred(ArchiveErrorCode::CorruptedArchive,
                           tr("Archive test failed"));
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
