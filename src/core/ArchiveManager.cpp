// QArchiver / 小Q解压
// ArchiveManager implementation: format detection, backend dispatch and retries.
// ArchiveManager 实现：格式检测、后端调度与重试。

#include "core/ArchiveManager.h"

#include "backends/ArchiveBackend.h"

#include "core/ArchiveBackendFactory.h"
#include "core/CompressOutput.h"
#include "core/ExtractionPreflight.h"

#include "utils/Logger.h"
#include "utils/MimeTypeHelper.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QTemporaryDir>
#include <QtConcurrent>

#include <algorithm>

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
    if (m_openingBackend) {
        m_openingBackend->requestCancel();
        disconnect(m_openingBackend, nullptr, this, nullptr);
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
    delete m_openingBackend;
    m_openingBackend = nullptr;

    cleanupCompressSession();
}

ArchiveManager &ArchiveManager::instance()
{
    static ArchiveManager manager;
    return manager;
}

void ArchiveManager::setSymlinkDecisionCallback(SymlinkDecisionCallback callback)
{
    m_symlinkDecisionCallback = std::move(callback);
}



void ArchiveManager::openArchive(const QString &filePath)
{
    if (m_busy) {
        emit errorOccurred(ArchiveErrorCode::Unknown,
                           tr("Another operation is already in progress"));
        return;
    }

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

    m_openingPath = filePath;
    m_openingFormat = ArchiveFormat::Unknown;
    m_opening = true;
    m_busy = true;
    m_pending = std::monostate{};
    m_retryCount = 0;
    m_preTestPending = false;

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
        m_openingPath.clear();
        m_openingFormat = ArchiveFormat::Unknown;
        m_busy = false;
        clearWriteAfterReopen();
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
                           tr("Unrecognized archive format or file is corrupted: %1")
                               .arg(filePath));
        return;
    }

    ArchiveBackend *backend = m_forceCliForOpen
        ? ArchiveBackendFactory::create(format)
        : ArchiveBackendFactory::createForRead(format, filePath);
    m_forceCliForOpen = false;
    if (!backend) {
        m_opening = false;
        m_openingPath.clear();
        m_openingFormat = ArchiveFormat::Unknown;
        m_busy = false;
        clearWriteAfterReopen();
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
                           tr("No backend available for format: %1")
                               .arg(formatToString(format)));
        return;
    }

    backend->moveToThread(&m_workerThread);
    m_openingBackend = backend;
    m_openingFormat = format;
    m_opening = true;
    m_busy = true;
    connectBackend(backend, m_generation);

    const bool invoked = QMetaObject::invokeMethod(
        backend,
        [backend, filePath]() { backend->openArchive(filePath); },
        Qt::QueuedConnection);

    if (!invoked) {
        m_opening = false;
        m_openingPath.clear();
        m_openingFormat = ArchiveFormat::Unknown;
        m_busy = false;
        clearWriteAfterReopen();
        ArchiveBackend *failed = m_openingBackend;
        m_openingBackend = nullptr;
        destroyBackend(failed);
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

    m_pending = options;
    m_retryCount = 0;

    if (!startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->listContents(options);
        })) {
        clearPendingState();
    }
}

bool ArchiveManager::validateExtraction(const ExtractOptions &options,
                                        QSet<QString> *skippedSymlinks)
{
    const ExtractionPreflight::Result result = ExtractionPreflight::check(
        options, m_entries, m_symlinkDecisionCallback);
    if (!result.ok) {
        emit errorOccurred(result.errorCode, result.errorMessage);
        return false;
    }
    if (skippedSymlinks) {
        *skippedSymlinks = result.skippedSymlinks;
    }
    return true;
}

void ArchiveManager::extract(const ExtractOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }

    ExtractOptions effective = options;
    QSet<QString> rejected;
    if (!validateExtraction(options, &rejected)) {
        return;
    }
    effective.skippedSymlinks.unite(rejected);
    effective.symlinkPreflightDone = true;

    if (!QDir().mkpath(effective.destDir)) {
        emit errorOccurred(ArchiveErrorCode::PermissionDenied,
                           tr("Cannot create destination directory: %1")
                               .arg(effective.destDir));
        return;
    }

    if (effective.testBeforeExtract) {
        m_preTestPending = true;
        m_preTestExtractOptions = effective;
        m_pending = effective;
        m_retryCount = 0;

        TestOptions testOptions;
        testOptions.password = effective.password;
        if (!startOperation(m_backend, [testOptions](ArchiveBackend *backend) {
                backend->testArchive(testOptions);
            })) {
            m_preTestPending = false;
            clearPendingState();
        }
        return;
    }

    startExtractOperation(effective);
}

void ArchiveManager::startExtractOperation(const ExtractOptions &options)
{
    m_pending = options;
    m_retryCount = 0;

    if (!startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->extract(options);
        })) {
        clearPendingState();
    }
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

    ArchiveBackend *backend = ArchiveBackendFactory::create(format);
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
    m_compressFormat = format;
    const QFileInfo finalInfo(m_compressFinalPath);
    if (!QDir().mkpath(finalInfo.absolutePath())) {
        emit errorOccurred(ArchiveErrorCode::PermissionDenied,
                           tr("Cannot create target directory: %1")
                               .arg(finalInfo.absolutePath()));
        cleanupTempBackend();
        cleanupCompressSession();
        return;
    }

    m_compressTargetExisted = CompressOutput::outputExists(finalInfo, format);
    CompressOptions effectiveOptions = options;
    effectiveOptions.archivePath = m_compressFinalPath;

    if (m_compressTargetExisted) {
        m_compressTempDir = std::make_unique<QTemporaryDir>(
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
        if (canReopenForWrite()) {
            LOG_INFO("ArchiveManager",
                     "Add files requested on a read-only backend; reopening %s with the CLI backend",
                     m_archivePath.toUtf8().constData());
            m_writeAfterReopen = options;
            m_forceCliForOpen = true;
            openArchive(m_archivePath);
            return;
        }
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("This archive does not support adding files"));
        return;
    }

    m_pending = options;
    m_retryCount = 0;

    if (!startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->addFiles(options);
        })) {
        clearPendingState();
    }
}

void ArchiveManager::deleteFiles(const DeleteFilesOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }
    if (!m_backend->supportsAppend()) {
        if (canReopenForWrite()) {
            LOG_INFO("ArchiveManager",
                     "Delete files requested on a read-only backend; reopening %s with the CLI backend",
                     m_archivePath.toUtf8().constData());
            m_writeAfterReopen = options;
            m_forceCliForOpen = true;
            openArchive(m_archivePath);
            return;
        }
        emit errorOccurred(ArchiveErrorCode::OperationNotSupported,
                           tr("This archive does not support deleting files"));
        return;
    }

    m_pending = options;
    m_retryCount = 0;

    if (!startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->deleteFiles(options);
        })) {
        clearPendingState();
    }
}

void ArchiveManager::testArchive(const TestOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }

    m_pending = options;
    m_retryCount = 0;

    if (!startOperation(m_backend, [options](ArchiveBackend *backend) {
            backend->testArchive(options);
        })) {
        clearPendingState();
    }
}

void ArchiveManager::retryWithPassword(const QString &password)
{
    if (!m_backend) {
        return;
    }

    if (m_retryCount >= 3) {
        clearPendingState();
        m_preTestPending = false;
        m_preTestExtractOptions = ExtractOptions{};
        emit errorOccurred(ArchiveErrorCode::WrongPassword,
                           tr("Too many failed password attempts."));
        return;
    }

    ++m_retryCount;

    if (auto *options = std::get_if<ListOptions>(&m_pending)) {
        options->password = password;
        const ListOptions opts = *options;
        if (!startOperation(m_backend, [opts](ArchiveBackend *backend) {
                backend->listContents(opts);
            })) {
            clearPendingState();
        }
        return;
    }
    if (auto *options = std::get_if<ExtractOptions>(&m_pending)) {
        options->password = password;
        extract(*options);
        return;
    }
    if (auto *options = std::get_if<TestOptions>(&m_pending)) {
        options->password = password;
        const TestOptions opts = *options;
        if (!startOperation(m_backend, [opts](ArchiveBackend *backend) {
                backend->testArchive(opts);
            })) {
            clearPendingState();
        }
        return;
    }
    if (auto *options = std::get_if<AddFilesOptions>(&m_pending)) {
        options->password = password;
        const AddFilesOptions opts = *options;
        if (!startOperation(m_backend, [opts](ArchiveBackend *backend) {
                backend->addFiles(opts);
            })) {
            clearPendingState();
        }
        return;
    }
    if (auto *options = std::get_if<DeleteFilesOptions>(&m_pending)) {
        options->password = password;
        const DeleteFilesOptions opts = *options;
        if (!startOperation(m_backend, [opts](ArchiveBackend *backend) {
                backend->deleteFiles(opts);
            })) {
            clearPendingState();
        }
        return;
    }
}

void ArchiveManager::cancelCurrentOperation()
{
    if (m_backend) {
        m_backend->requestCancel();
    }
    if (m_openingBackend) {
        m_openingBackend->requestCancel();
    }
    if (m_tempBackend) {
        m_tempBackend->requestCancel();
    }
}

void ArchiveManager::cancelPendingOperation()
{
    clearPendingState();
    m_preTestPending = false;
    m_preTestExtractOptions = ExtractOptions{};
    m_busy = false;
    emit operationFinished(false, tr("Cancelled"));
}

bool ArchiveManager::canModifyCurrentArchive() const
{
    return m_backend
        && (m_backend->supportsAppend() || canReopenForWrite());
}

bool ArchiveManager::canReopenForWrite() const
{
    if (m_usingLibArchive == false || m_archivePath.isEmpty()) {
        return false;
    }
    if (MimeTypeHelper::isSplitVolume(m_archivePath)) {
        return false;
    }
    return m_format == ArchiveFormat::Zip
        || m_format == ArchiveFormat::SevenZip
        || m_format == ArchiveFormat::Tar;
}

void ArchiveManager::clearWriteAfterReopen()
{
    m_writeAfterReopen = std::monostate{};
    m_forceCliForOpen = false;
}


void ArchiveManager::closeArchive()
{
    ++m_generation;
    m_busy = false;
    m_opening = false;
    clearPendingState();
    m_preTestPending = false;
    m_preTestExtractOptions = ExtractOptions{};
    m_archivePath.clear();
    m_openingPath.clear();
    m_format = ArchiveFormat::Unknown;
    m_openingFormat = ArchiveFormat::Unknown;
    m_usingLibArchive = false;
    m_fallbackDone = false;
    m_fallbackInProgress = false;
    clearWriteAfterReopen();

    ArchiveBackend *opening = m_openingBackend;
    m_openingBackend = nullptr;
    destroyBackend(opening);

    ArchiveBackend *current = m_backend;
    m_backend = nullptr;
    m_entries.clear();
    destroyBackend(current);

    QTemporaryDir *tempDir = m_compressTempDir.release();
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

void ArchiveManager::destroyBackend(ArchiveBackend *backend)
{
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
}

void ArchiveManager::clearPendingState()
{
    m_pending = std::monostate{};
    m_retryCount = 0;
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
    return CompressOutput::promote(QFileInfo(m_compressFinalPath),
                                   m_compressFormat,
                                   m_compressTempDir->path(),
                                   m_compressTargetExisted);
}

void ArchiveManager::removeFailedCompressOutput()
{
    if (m_compressTempDir || m_compressFinalPath.isEmpty()) {
        return;
    }
    CompressOutput::removeOutput(QFileInfo(m_compressFinalPath), m_compressFormat);
}

void ArchiveManager::cleanupCompressSession()
{
    m_compressTempDir.reset();
    m_compressFinalPath.clear();
    m_compressFormat = ArchiveFormat::Unknown;
    m_compressTargetExisted = false;
}
