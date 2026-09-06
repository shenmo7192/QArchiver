#include "ArchiveWorker.h"
#include "backends/ArchiveBackend.h"
#include "core/ArchiveError.h"

#include <QtConcurrent>

ArchiveWorker::ArchiveWorker(ArchiveBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
{
}

void ArchiveWorker::runList(const ListOptions &options)
{
    if (!m_backend) {
        emit error(ArchiveErrorCode::Unknown, tr("No backend bound"));
        emit finished();
        return;
    }
    ArchiveBackend *backend = m_backend.data();
    ListOptions opts = options;
    QtConcurrent::run([backend, opts, this]() {
        try {
            backend->listContents(opts);
        } catch (const ArchiveException &e) {
            emit error(e.code(), e.message());
        } catch (...) {
            emit error(ArchiveErrorCode::Unknown, tr("Unknown error during list"));
        }
        emit finished();
    });
}

void ArchiveWorker::runExtract(const ExtractOptions &options)
{
    if (!m_backend) {
        emit error(ArchiveErrorCode::Unknown, tr("No backend bound"));
        emit finished();
        return;
    }
    ArchiveBackend *backend = m_backend.data();
    ExtractOptions opts = options;
    QtConcurrent::run([backend, opts, this]() {
        try {
            backend->extract(opts);
        } catch (const ArchiveException &e) {
            emit error(e.code(), e.message());
        } catch (...) {
            emit error(ArchiveErrorCode::Unknown, tr("Unknown error during extract"));
        }
        emit finished();
    });
}

void ArchiveWorker::runCompress(const CompressOptions &options)
{
    if (!m_backend) {
        emit error(ArchiveErrorCode::Unknown, tr("No backend bound"));
        emit finished();
        return;
    }
    ArchiveBackend *backend = m_backend.data();
    CompressOptions opts = options;
    QtConcurrent::run([backend, opts, this]() {
        try {
            backend->compress(opts);
        } catch (const ArchiveException &e) {
            emit error(e.code(), e.message());
        } catch (...) {
            emit error(ArchiveErrorCode::Unknown, tr("Unknown error during compress"));
        }
        emit finished();
    });
}

void ArchiveWorker::runTest(const TestOptions &options)
{
    if (!m_backend) {
        emit error(ArchiveErrorCode::Unknown, tr("No backend bound"));
        emit finished();
        return;
    }
    ArchiveBackend *backend = m_backend.data();
    TestOptions opts = options;
    QtConcurrent::run([backend, opts, this]() {
        try {
            backend->testArchive(opts);
        } catch (const ArchiveException &e) {
            emit error(e.code(), e.message());
        } catch (...) {
            emit error(ArchiveErrorCode::Unknown, tr("Unknown error during test"));
        }
        emit finished();
    });
}

void ArchiveWorker::runAddFiles(const AddFilesOptions &options)
{
    if (!m_backend) {
        emit error(ArchiveErrorCode::Unknown, tr("No backend bound"));
        emit finished();
        return;
    }
    ArchiveBackend *backend = m_backend.data();
    AddFilesOptions opts = options;
    QtConcurrent::run([backend, opts, this]() {
        try {
            backend->addFiles(opts);
        } catch (const ArchiveException &e) {
            emit error(e.code(), e.message());
        } catch (...) {
            emit error(ArchiveErrorCode::Unknown, tr("Unknown error during add"));
        }
        emit finished();
    });
}

void ArchiveWorker::runDeleteFiles(const DeleteFilesOptions &options)
{
    if (!m_backend) {
        emit error(ArchiveErrorCode::Unknown, tr("No backend bound"));
        emit finished();
        return;
    }
    ArchiveBackend *backend = m_backend.data();
    DeleteFilesOptions opts = options;
    QtConcurrent::run([backend, opts, this]() {
        try {
            backend->deleteFiles(opts);
        } catch (const ArchiveException &e) {
            emit error(e.code(), e.message());
        } catch (...) {
            emit error(ArchiveErrorCode::Unknown, tr("Unknown error during delete"));
        }
        emit finished();
    });
}
