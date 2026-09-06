#include "ArchiveManager.h"
#include "ArchiveWorker.h"

#include "backends/ArchiveBackend.h"
#include "backends/tar/TarBackend.h"
#include "backends/zip/ZipBackend.h"
#include "backends/sevenzip/SevenZipBackend.h"
#include "backends/rar/RarBackend.h"
#include "backends/stream/StreamBackend.h"

#include "utils/MimeTypeHelper.h"
#include "utils/Logger.h"
#include "utils/FileUtils.h"
#include "core/RecentFilesManager.h"

#include <QFileInfo>
#include <QtConcurrent>

ArchiveManager::ArchiveManager()
{
}

ArchiveManager &ArchiveManager::instance()
{
    static ArchiveManager s;
    return s;
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
    case ArchiveFormat::Rar5:
        return new RarBackend(fmt);
    case ArchiveFormat::Gzip:
    case ArchiveFormat::Bzip2:
    case ArchiveFormat::Xz:
        return new StreamBackend(fmt);
    case ArchiveFormat::Unknown:
        return nullptr;
    }
    return nullptr;
}

void ArchiveManager::connectBackend(ArchiveBackend *backend)
{
    if (!backend) return;
    connect(backend, &ArchiveBackend::archiveOpened, this, &ArchiveManager::archiveOpened);
    connect(backend, &ArchiveBackend::fileListReady, this, &ArchiveManager::fileListReady);
    connect(backend, &ArchiveBackend::progressChanged, this, &ArchiveManager::progressChanged);
    connect(backend, &ArchiveBackend::outputReady, this, &ArchiveManager::outputReady);
    connect(backend, &ArchiveBackend::passwordRequested, this, &ArchiveManager::passwordRequested);
    connect(backend, &ArchiveBackend::errorOccurred, this, &ArchiveManager::errorOccurred);
    connect(backend, &ArchiveBackend::operationFinished, this, &ArchiveManager::operationFinished);
}

void ArchiveManager::deleteBackend()
{
    if (m_backend) {
        m_backend->closeArchive();
        delete m_backend;
        m_backend = nullptr;
    }
    m_archivePath.clear();
    m_format = ArchiveFormat::Unknown;
}

void ArchiveManager::openArchive(const QString &filePath)
{
    deleteBackend();

    QFileInfo fi(filePath);
    if (!fi.exists()) {
        emit errorOccurred(ArchiveErrorCode::FileNotFound,
            tr("File does not exist: %1").arg(filePath));
        return;
    }

    // Detect format
    m_format = MimeTypeHelper::detect(filePath);
    if (m_format == ArchiveFormat::Unknown) {
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
            tr("Unrecognized archive format or file is corrupted: %1").arg(filePath));
        return;
    }

    LOG_INFO("ArchiveManager", "Detected format %s for %s",
             formatToString(m_format).toUtf8().constData(),
             filePath.toUtf8().constData());

    // Create backend
    m_backend = createBackend(m_format);
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
            tr("No backend available for format: %1").arg(formatToString(m_format)));
        return;
    }
    connectBackend(m_backend);
    m_archivePath = filePath;

    // Record in recent files
    RecentFilesManager::instance().add(filePath);

    // Open (lightweight bind)
    m_backend->openArchive(filePath);
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

    // Run asynchronously
    auto *backend = m_backend;
    ListOptions opts = options;
    QtConcurrent::run([backend, opts]() {
        backend->listContents(opts);
    });
}

void ArchiveManager::extract(const ExtractOptions &options)
{
    if (!m_backend) {
        emit errorOccurred(ArchiveErrorCode::Unknown, tr("No archive open"));
        return;
    }

    // Disk space pre-check
    qint64 freeSpace = FileUtils::estimateFreeSpace(options.destDir);
    if (freeSpace >= 0 && freeSpace < 1024 * 1024) {
        emit errorOccurred(ArchiveErrorCode::InsufficientDiskSpace,
            tr("Insufficient disk space on destination"));
        return;
    }

    m_pendingOp = PendingOp::Extract;
    m_retryCount = 0;
    m_pendingExtractOptions = options;

    auto *backend = m_backend;
    ExtractOptions opts = options;
    QtConcurrent::run([backend, opts]() {
        backend->extract(opts);
    });
}

void ArchiveManager::compress(const CompressOptions &options, ArchiveFormat fmt)
{
    // Create a fresh backend for compression (not tied to an open archive)
    ArchiveBackend *backend = createBackend(fmt);
    if (!backend) {
        emit errorOccurred(ArchiveErrorCode::UnsupportedFormat,
            tr("No backend available for format: %1").arg(formatToString(fmt)));
        return;
    }
    // Connect signals (temporary backend)
    connect(backend, &ArchiveBackend::progressChanged, this, &ArchiveManager::progressChanged);
    connect(backend, &ArchiveBackend::outputReady, this, &ArchiveManager::outputReady);
    connect(backend, &ArchiveBackend::errorOccurred, this, &ArchiveManager::errorOccurred);
    connect(backend, &ArchiveBackend::operationFinished, this, [this, backend](bool ok, const QString &msg) {
        emit operationFinished(ok, msg);
        backend->deleteLater();
    });

    CompressOptions opts = options;
    QtConcurrent::run([backend, opts]() {
        backend->compress(opts);
    });
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

    auto *backend = m_backend;
    AddFilesOptions opts = options;
    QtConcurrent::run([backend, opts]() {
        backend->addFiles(opts);
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

    auto *backend = m_backend;
    DeleteFilesOptions opts = options;
    QtConcurrent::run([backend, opts]() {
        backend->deleteFiles(opts);
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

    auto *backend = m_backend;
    TestOptions opts = options;
    QtConcurrent::run([backend, opts]() {
        backend->testArchive(opts);
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
    auto *backend = m_backend;

    switch (m_pendingOp) {
    case PendingOp::List: {
        ListOptions opts = m_pendingListOptions;
        opts.password = password;
        m_pendingListOptions = opts;
        QtConcurrent::run([backend, opts]() { backend->listContents(opts); });
        break;
    }
    case PendingOp::Extract: {
        ExtractOptions opts = m_pendingExtractOptions;
        opts.password = password;
        m_pendingExtractOptions = opts;
        QtConcurrent::run([backend, opts]() { backend->extract(opts); });
        break;
    }
    case PendingOp::Test: {
        TestOptions opts = m_pendingTestOptions;
        opts.password = password;
        m_pendingTestOptions = opts;
        QtConcurrent::run([backend, opts]() { backend->testArchive(opts); });
        break;
    }
    case PendingOp::Add: {
        AddFilesOptions opts = m_pendingAddOptions;
        opts.password = password;
        m_pendingAddOptions = opts;
        QtConcurrent::run([backend, opts]() { backend->addFiles(opts); });
        break;
    }
    case PendingOp::Delete: {
        DeleteFilesOptions opts = m_pendingDeleteOptions;
        opts.password = password;
        m_pendingDeleteOptions = opts;
        QtConcurrent::run([backend, opts]() { backend->deleteFiles(opts); });
        break;
    }
    case PendingOp::None:
        break;
    }
}

void ArchiveManager::closeArchive()
{
    deleteBackend();
}
