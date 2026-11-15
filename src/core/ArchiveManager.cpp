#include "ArchiveManager.h"

#include "backends/ArchiveBackend.h"
#include "backends/rar/RarBackend.h"
#include "backends/sevenzip/SevenZipBackend.h"
#include "backends/stream/StreamBackend.h"
#include "backends/tar/TarBackend.h"
#include "backends/zip/ZipBackend.h"

#include "core/RecentFilesManager.h"
#include "utils/FileUtils.h"
#include "utils/Logger.h"
#include "utils/MimeTypeHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLocale>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QtConcurrent>

ArchiveManager::ArchiveManager()
{
    m_workerThread.setObjectName(QStringLiteral("ArchiveWorker"));
    m_workerThread.start();
}

ArchiveManager::~ArchiveManager()
{
    ++m_generation;

    if (m_backend) {
        m_backend->requestCancel();
        disconnect(m_backend, nullptr, this, nullptr);
    }
    if (m_tempBackend) {
        m_tempBackend->requestCancel();
        disconnect(m_tempBackend, nullptr, this, nullptr);
    }

    m_workerThread.quit();
    if (!m_workerThread.wait(5000)) {
        m_workerThread.terminate();
        m_workerThread.wait(1000);
    }

    delete m_backend;
    m_backend = nullptr;
    delete m_tempBackend;
    m_tempBackend = nullptr;

    cleanupCompressSession();
}

ArchiveManager &ArchiveManager::instance()
{
    static ArchiveManager manager;
    return manager;
}

ArchiveBackend *ArchiveManager::createBackend(ArchiveFormat fmt)
{
    switch (fmt) {
    case ArchiveFormat::Tar:
    case ArchiveFormat::TarGz:
    case ArchiveFormat::TarXz:
    case ArchiveFormat::TarBz2:
        return new TarBackend(fmt);
    case ArchiveFormat::Zip:
        return new ZipBackend();
    case ArchiveFormat::SevenZip:
        return new SevenZipBackend();
    case ArchiveFormat::Rar4:
        return new RarBackend(ArchiveFormat::Rar4);
    case ArchiveFormat::Rar5:
        return new RarBackend(ArchiveFormat::Rar5);
    case ArchiveFormat::Gzip:
    case ArchiveFormat::Bzip2:
    case ArchiveFormat::Xz:
        return new StreamBackend(fmt);
    case ArchiveFormat::Unknown:
        return nullptr;
    }
    return nullptr;
}

bool ArchiveManager::startOperation(ArchiveBackend *backend,
                                    std::function<void(ArchiveBackend *)> operation)
{
    if (!backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return false;
    }
    if (m_busy) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Another operation is already in progress"));
        return false;
    }

    m_busy = true;
    backend->clearCancel();

    const bool invoked = QMetaObject::invokeMethod(
        backend,
        [backend, operation = std::move(operation)]() { operation(backend); },
        Qt::QueuedConnection);

    if (!invoked) {
        m_busy = false;
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Failed to dispatch the archive operation"));
    }
    return invoked;
}

void ArchiveManager::connectBackend(ArchiveBackend *backend, quint64 generation)
{
    const auto isCurrent = [this, generation]() { return generation == m_generation; };

    connect(backend, &ArchiveBackend::archiveOpened, this,
            [this, isCurrent](const QString &filePath) {
                if (!isCurrent()) {
                    return;
                }
                m_opening = false;
                RecentFilesManager::instance().add(filePath);
                emit archiveOpened(filePath);
            });

    connect(backend, &ArchiveBackend::fileListReady, this,
            [this, isCurrent](const QList<ArchiveEntry> &entries) {
                if (isCurrent()) {
                    m_entries = entries;
                    m_busy = false;
                    m_pendingOp = PendingOp::None;
                    m_retryCount = 0;
                    emit fileListReady(entries);
                }
            });

    connect(backend, &ArchiveBackend::progressChanged, this,
            [this, isCurrent](int percent, int total, int current, int etaSec) {
                if (isCurrent()) {
                    emit progressChanged(percent, total, current, etaSec);
                }
            });

    connect(backend, &ArchiveBackend::outputReady, this,
            [this, isCurrent](const QString &line) {
                if (isCurrent()) {
                    emit outputReady(line);
                }
            });

    connect(backend, &ArchiveBackend::passwordRequested, this,
            [this, isCurrent]() {
                if (!isCurrent()) {
                    return;
                }
                m_busy = false;
                emit passwordRequested();
            });

    connect(backend, &ArchiveBackend::operationFinished, this,
            [this, backend, isCurrent](bool success, const QString &msg) {
                if (isCurrent()) {
                    handleTerminal(backend, success, msg);
                }
            });

    connect(backend, &ArchiveBackend::errorOccurred, this,
            [this, backend, isCurrent](ArchiveErrorCode code, const QString &msg) {
                if (isCurrent()) {
                    handleError(backend, code, msg);
                }
            });
}

void ArchiveManager::connectTempBackend(ArchiveBackend *backend)
{
    connect(backend, &ArchiveBackend::progressChanged, this,
            [this, backend](int percent, int total, int current, int etaSec) {
                if (backend == m_tempBackend) {
                    emit progressChanged(percent, total, current, etaSec);
                }
            });

    connect(backend, &ArchiveBackend::outputReady, this,
            [this, backend](const QString &line) {
                if (backend == m_tempBackend) {
                    emit outputReady(line);
                }
            });

    connect(backend, &ArchiveBackend::operationFinished, this,
            [this, backend](bool success, const QString &msg) {
                if (backend == m_tempBackend) {
                    handleTerminal(backend, success, msg);
                }
            });

    connect(backend, &ArchiveBackend::errorOccurred, this,
            [this, backend](ArchiveErrorCode code, const QString &msg) {
                if (backend == m_tempBackend) {
                    handleError(backend, code, msg);
                }
            });
}

void ArchiveManager::openArchive(const QString &filePath)
{
    closeArchive();

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

    ++m_generation;
    const quint64 generation = m_generation;

    m_archivePath = filePath;
    m_format = ArchiveFormat::Unknown;
    m_opening = true;
    m_busy = true;

    emit archiveOpening(filePath);

    auto *watcher = new QFutureWatcher<ArchiveFormat>(this);
    connect(watcher, &QFutureWatcher<ArchiveFormat>::finished, this,
            [this, watcher, filePath, generation]() {
                ArchiveFormat format = ArchiveFormat::Unknown;
                try {
                    format = watcher->result();
                } catch (...) {
                    format = ArchiveFormat::Unknown;
                }
                watcher->deleteLater();

                if (generation != m_generation) {
                    return;
                }
                m_busy = false;
                finishOpen(filePath, format);
            });

    watcher->setFuture(QtConcurrent::run(
        [filePath]() { return MimeTypeHelper::detect(filePath); }));
}

void ArchiveManager::finishOpen(const QString &filePath, ArchiveFormat format)
{
    if (format == ArchiveFormat::Unknown) {
        m_opening = false;
        m_archivePath.clear();
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
                           tr("Unrecognized archive format or file is corrupted: %1")
                               .arg(filePath));
        return;
    }

    m_format = format;
    ArchiveBackend *backend = createBackend(format);
    if (!backend) {
        m_opening = false;
        m_archivePath.clear();
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
                           tr("No backend available for format: %1")
                               .arg(formatToString(format)));
        return;
    }

    backend->moveToThread(&m_workerThread);
    m_backend = backend;
    m_opening = true;
    connectBackend(backend, m_generation);

    const bool invoked = QMetaObject::invokeMethod(
        backend,
        [backend, filePath]() { backend->openArchive(filePath); },
        Qt::QueuedConnection);

    if (!invoked) {
        m_opening = false;
        closeArchive();
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Failed to open archive: %1").arg(filePath));
    }
}

void ArchiveManager::listContents(const ListOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }

    m_pendingOp = PendingOp::List;
    m_retryCount = 0;
    m_pendingListOptions = options;

    startOperation(m_backend, [options](ArchiveBackend *backend) {
        backend->listContents(options);
    });
}

bool ArchiveManager::validateExtraction(const ExtractOptions &options)
{
    if (options.destDir.trimmed().isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("No destination directory selected"));
        return false;
    }

    const QStringList pathsToCheck = options.selectedFiles.isEmpty()
        ? QStringList()
        : options.selectedFiles;

    if (!pathsToCheck.isEmpty()) {
        for (const QString &path : pathsToCheck) {
            if (FileUtils::isPathTraversal(path, options.destDir)) {
                emit errorOccurred(ArchiveErrorCode::PathTraversalBlocked,
                                   tr("Blocked unsafe path: %1").arg(path));
                return false;
            }
        }
    } else {
        for (const ArchiveEntry &entry : m_entries) {
            if (FileUtils::isPathTraversal(entry.path, options.destDir)) {
                emit errorOccurred(ArchiveErrorCode::PathTraversalBlocked,
                                   tr("Blocked unsafe path: %1").arg(entry.path));
                return false;
            }
        }
    }

    const qint64 required = FileUtils::totalUnpackedSize(m_entries);
    const qint64 available = FileUtils::estimateFreeSpace(options.destDir);
    if (required > 0 && available >= 0 && available < required + 1024 * 1024) {
        emit errorOccurred(ArchiveErrorCode::InsufficientDiskSpace,
                           tr("Insufficient disk space: %1 required, %2 available")
                               .arg(QLocale().formattedDataSize(required),
                                    QLocale().formattedDataSize(available)));
        return false;
    }

    return true;
}

void ArchiveManager::extract(const ExtractOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }

    if (!validateExtraction(options)) {
        return;
    }

    if (!QDir().mkpath(options.destDir)) {
        emit errorOccurred(ArchiveErrorCode::PermissionDenied,
                           tr("Cannot create destination directory: %1").arg(options.destDir));
        return;
    }

    m_pendingOp = PendingOp::Extract;
    m_retryCount = 0;
    m_pendingExtractOptions = options;

    if (options.testBeforeExtract) {
        m_preTestPending = true;
        m_preTestExtractOptions = options;

        TestOptions testOptions;
        testOptions.password = options.password;
        startOperation(m_backend, [testOptions](ArchiveBackend *backend) {
            backend->testArchive(testOptions);
        });
        return;
    }

    startExtractOperation(options);
}

void ArchiveManager::startExtractOperation(const ExtractOptions &options)
{
    m_pendingOp = PendingOp::Extract;
    m_pendingExtractOptions = options;

    startOperation(m_backend, [options](ArchiveBackend *backend) {
        backend->extract(options);
    });
}

void ArchiveManager::compress(const CompressOptions &options, ArchiveFormat format)
{
    if (m_busy) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Another operation is already in progress"));
        return;
    }
    if (options.archivePath.trimmed().isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive name specified"));
        return;
    }
    if (options.sourceFiles.isEmpty()) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No source files selected"));
        return;
    }

    cleanupCompressSession();

    ArchiveBackend *backend = createBackend(format);
    if (!backend) {
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
                           tr("No backend available for format: %1")
                               .arg(formatToString(format)));
        return;
    }

    backend->moveToThread(&m_workerThread);
    m_tempBackend = backend;
    connectTempBackend(backend);

    m_compressFinalPath = QFileInfo(options.archivePath).absoluteFilePath();
    const QFileInfo finalInfo(m_compressFinalPath);
    if (!QDir().mkpath(finalInfo.absolutePath())) {
        emit errorOccurred(ArchiveErrorCode::PermissionDenied,
                           tr("Cannot create target directory: %1")
                               .arg(finalInfo.absolutePath()));
        cleanupTempBackend();
        cleanupCompressSession();
        return;
    }

    m_compressTargetExisted = finalInfo.exists();
    CompressOptions effectiveOptions = options;
    effectiveOptions.archivePath = m_compressFinalPath;

    if (m_compressTargetExisted) {
        m_compressTempDir = new QTemporaryDir(
            QDir(finalInfo.absolutePath())
                .filePath(QStringLiteral(".qarchiver_compress_XXXXXX")));
        if (!m_compressTempDir->isValid()) {
            emit errorOccurred(ArchiveErrorCode::PermissionDenied,
                               tr("Cannot create a temporary directory next to the target"));
            cleanupTempBackend();
            cleanupCompressSession();
            return;
        }
        effectiveOptions.archivePath =
            QDir(m_compressTempDir->path()).filePath(finalInfo.fileName());
    }

    m_busy = true;
    backend->clearCancel();

    const bool invoked = QMetaObject::invokeMethod(
        backend,
        [backend, effectiveOptions]() { backend->compress(effectiveOptions); },
        Qt::QueuedConnection);

    if (!invoked) {
        m_busy = false;
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Failed to dispatch the compression operation"));
        cleanupTempBackend();
        cleanupCompressSession();
    }
}

void ArchiveManager::addFiles(const AddFilesOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }
    if (!m_backend->supportsAppend()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("This archive does not support adding files"));
        return;
    }

    m_pendingOp = PendingOp::Add;
    m_retryCount = 0;
    m_pendingAddOptions = options;

    startOperation(m_backend, [options](ArchiveBackend *backend) {
        backend->addFiles(options);
    });
}

void ArchiveManager::deleteFiles(const DeleteFilesOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }
    if (!m_backend->supportsAppend()) {
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("This archive does not support deleting files"));
        return;
    }

    m_pendingOp = PendingOp::Delete;
    m_retryCount = 0;
    m_pendingDeleteOptions = options;

    startOperation(m_backend, [options](ArchiveBackend *backend) {
        backend->deleteFiles(options);
    });
}

void ArchiveManager::testArchive(const TestOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }

    m_pendingOp = PendingOp::Test;
    m_retryCount = 0;
    m_pendingTestOptions = options;

    startOperation(m_backend, [options](ArchiveBackend *backend) {
        backend->testArchive(options);
    });
}

void ArchiveManager::retryWithPassword(const QString &password)
{
    if (!m_backend) {
        return;
    }

    if (m_retryCount >= 3) {
        m_pendingOp = PendingOp::None;
        m_retryCount = 0;
        emit errorOccurred(ArchiveErrorCode::WrongPassword,
                           tr("Too many failed password attempts."));
        return;
    }

    ++m_retryCount;

    switch (m_pendingOp) {
    case PendingOp::List: {
        m_pendingListOptions.password = password;
        const ListOptions options = m_pendingListOptions;
        startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->listContents(options);
        });
        break;
    }
    case PendingOp::Extract: {
        m_pendingExtractOptions.password = password;
        extract(m_pendingExtractOptions);
        break;
    }
    case PendingOp::Test: {
        m_pendingTestOptions.password = password;
        const TestOptions options = m_pendingTestOptions;
        startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->testArchive(options);
        });
        break;
    }
    case PendingOp::Add: {
        m_pendingAddOptions.password = password;
        const AddFilesOptions options = m_pendingAddOptions;
        startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->addFiles(options);
        });
        break;
    }
    case PendingOp::Delete: {
        m_pendingDeleteOptions.password = password;
        const DeleteFilesOptions options = m_pendingDeleteOptions;
        startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->deleteFiles(options);
        });
        break;
    }
    case PendingOp::None:
        break;
    }
}

void ArchiveManager::cancelCurrentOperation()
{
    if (m_backend) {
        m_backend->requestCancel();
    }
    if (m_tempBackend) {
        m_tempBackend->requestCancel();
    }
}

void ArchiveManager::cancelPendingOperation()
{
    m_pendingOp = PendingOp::None;
    m_retryCount = 0;
    m_busy = false;
    emit operationFinished(false, tr("Cancelled"));
}

void ArchiveManager::handleTerminal(ArchiveBackend *backend, bool success,
                                    const QString &msg)
{
    m_busy = false;

    if (backend == m_tempBackend) {
        bool promoted = true;
        if (success) {
            promoted = promoteCompressedArchive();
        } else {
            removeFailedCompressOutput();
        }

        const bool finalSuccess = success && promoted;
        emit operationFinished(finalSuccess,
                               finalSuccess ? msg
                                            : tr("Failed to finalize the compressed archive"));
        cleanupTempBackend();
        cleanupCompressSession();
        return;
    }

    if (m_preTestPending) {
        m_preTestPending = false;
        if (!success) {
            m_pendingOp = PendingOp::None;
            m_retryCount = 0;
            emit operationFinished(false, msg);
            return;
        }

        ExtractOptions options = m_preTestExtractOptions;
        options.testBeforeExtract = false;
        startExtractOperation(options);
        return;
    }

    m_pendingOp = PendingOp::None;
    m_retryCount = 0;
    emit operationFinished(success, msg);
}

void ArchiveManager::handleError(ArchiveBackend *backend, ArchiveErrorCode code,
                                 const QString &msg)
{
    m_busy = false;

    if (backend == m_tempBackend) {
        removeFailedCompressOutput();
        emit errorOccurred(code, msg);
        cleanupTempBackend();
        cleanupCompressSession();
        return;
    }

    if (m_preTestPending) {
        m_preTestPending = false;
        m_pendingOp = PendingOp::None;
        m_retryCount = 0;
    }

    if (m_opening) {
        m_opening = false;
        closeArchive();
    } else {
        m_pendingOp = PendingOp::None;
        m_retryCount = 0;
    }

    emit errorOccurred(code, msg);
}

void ArchiveManager::closeArchive()
{
    ++m_generation;
    m_busy = false;
    m_opening = false;
    m_pendingOp = PendingOp::None;
    m_retryCount = 0;
    m_preTestPending = false;
    m_archivePath.clear();
    m_format = ArchiveFormat::Unknown;

    auto destroyBackend = [this](ArchiveBackend *backend) {
        if (!backend) {
            return;
        }
        backend->requestCancel();
        disconnect(backend, nullptr, this, nullptr);
        if (m_workerThread.isRunning()) {
            QMetaObject::invokeMethod(
                backend,
                [backend]() {
                    backend->closeArchive();
                    delete backend;
                },
                Qt::QueuedConnection);
        } else {
            backend->closeArchive();
            delete backend;
        }
    };

    ArchiveBackend *current = m_backend;
    m_backend = nullptr;
    m_entries.clear();
    destroyBackend(current);

    QTemporaryDir *tempDir = m_compressTempDir;
    m_compressTempDir = nullptr;
    ArchiveBackend *tempBackend = m_tempBackend;
    m_tempBackend = nullptr;

    if (tempBackend) {
        tempBackend->requestCancel();
        disconnect(tempBackend, nullptr, this, nullptr);
        if (m_workerThread.isRunning()) {
            QMetaObject::invokeMethod(
                tempBackend,
                [tempBackend, tempDir]() {
                    tempBackend->closeArchive();
                    delete tempBackend;
                    delete tempDir;
                },
                Qt::QueuedConnection);
        } else {
            tempBackend->closeArchive();
            delete tempBackend;
            delete tempDir;
        }
    } else {
        delete tempDir;
    }

    m_compressFinalPath.clear();
    m_compressTargetExisted = false;
}

void ArchiveManager::cleanupTempBackend()
{
    ArchiveBackend *backend = m_tempBackend;
    m_tempBackend = nullptr;
    if (!backend) {
        return;
    }

    disconnect(backend, nullptr, this, nullptr);
    if (m_workerThread.isRunning()) {
        QMetaObject::invokeMethod(
            backend,
            [backend]() {
                backend->closeArchive();
                delete backend;
            },
            Qt::QueuedConnection);
    } else {
        backend->closeArchive();
        delete backend;
    }
}

bool ArchiveManager::promoteCompressedArchive()
{
    if (!m_compressTempDir) {
        return true;
    }

    const QFileInfo finalInfo(m_compressFinalPath);
    QDir finalDir(finalInfo.absolutePath());
    QDir tempDir(m_compressTempDir->path());

    const QString baseName = finalInfo.fileName();
    const QRegularExpression targetRe(
        QStringLiteral("^%1(?:\\.\\d{3,})?$").arg(QRegularExpression::escape(baseName)),
        QRegularExpression::CaseInsensitiveOption);

    if (m_compressTargetExisted) {
        const QStringList existing = finalDir.entryList(QDir::Files);
        for (const QString &name : existing) {
            if (targetRe.match(name).hasMatch()) {
                QFile::remove(finalDir.filePath(name));
            }
        }
    }

    const QStringList produced = tempDir.entryList(QDir::Files);
    if (produced.isEmpty()) {
        return false;
    }

    for (const QString &name : produced) {
        const QString source = tempDir.filePath(name);
        const QString destination = finalDir.filePath(name);
        if (QFileInfo::exists(destination)) {
            QFile::remove(destination);
        }
        if (!QFile::rename(source, destination)) {
            if (!QFile::copy(source, destination) || !QFile::remove(source)) {
                return false;
            }
        }
    }
    return true;
}

void ArchiveManager::removeFailedCompressOutput()
{
    if (m_compressTempDir) {
        return;
    }
    if (m_compressFinalPath.isEmpty()) {
        return;
    }

    const QFileInfo finalInfo(m_compressFinalPath);
    QDir finalDir(finalInfo.absolutePath());
    const QRegularExpression targetRe(
        QStringLiteral("^%1(?:\\.\\d{3,})?$")
            .arg(QRegularExpression::escape(finalInfo.fileName())),
        QRegularExpression::CaseInsensitiveOption);

    const QStringList existing = finalDir.entryList(QDir::Files);
    for (const QString &name : existing) {
        if (targetRe.match(name).hasMatch()) {
            QFile::remove(finalDir.filePath(name));
        }
    }
}

void ArchiveManager::cleanupCompressSession()
{
    delete m_compressTempDir;
    m_compressTempDir = nullptr;
    m_compressFinalPath.clear();
    m_compressTargetExisted = false;
}
