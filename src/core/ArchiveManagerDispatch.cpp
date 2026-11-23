// QArchiver / 小Q解压
// ArchiveManager worker-thread dispatch: operation start, signal wiring,
// terminal handling and error/fallback routing.
// ArchiveManager 工作线程调度：操作启动、信号连接、终止处理与错误/回退分发。

#include "core/ArchiveManager.h"

#include "backends/ArchiveBackend.h"
#include "common/FormatRegistry.h"

#include "utils/FileUtils.h"
#include "utils/Logger.h"
#include "utils/MimeTypeHelper.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QtConcurrent>

#include "core/ArchiveBackendFactory.h"
#include "core/RecentFilesManager.h"

#ifdef QARCHIVER_HAS_LIBARCHIVE
#include "backends/libarchive/LibArchiveBackend.h"
#endif

namespace {

// Variant visitor helper for the pending-operation dispatch.
template <typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <typename... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

} // namespace

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
            [this, backend, isCurrent](const QString &filePath) {
                if (!isCurrent()) {
                    return;
                }
                if (backend == m_openingBackend) {
                    // The new archive opened successfully; only now replace
                    // the previously open archive, so a failure leaves it
                    // untouched.
                    // 新压缩包成功打开后才替换旧压缩包；失败时旧包保持不变。
                    ArchiveBackend *old = m_backend;
                    m_backend = backend;
                    m_openingBackend = nullptr;
                    m_archivePath = m_openingPath;
                    m_format = m_openingFormat;
                    m_openingPath.clear();
                    m_openingFormat = ArchiveFormat::Unknown;
                    m_opening = false;
                    m_busy = false;
                    m_entries.clear();
                    m_pending = std::monostate{};
                    m_retryCount = 0;
#ifdef QARCHIVER_HAS_LIBARCHIVE
                    m_usingLibArchive =
                        qobject_cast<LibArchiveBackend *>(backend) != nullptr;
#else
                    m_usingLibArchive = false;
#endif
                    m_fallbackDone = false;
                    m_fallbackInProgress = false;
                    if (old && old != backend) {
                        destroyBackend(old);
                    }

                    // A read-only libarchive backend was reopened with the CLI
                    // to replay add/delete.  Dispatch that write now; the UI is
                    // still showing its progress dialog and will reload the list
                    // when operationFinished arrives.
                    // 为执行添加/删除而用命令行后端重新打开了只读的 libarchive
                    // 后端。现在派发该写操作；界面仍显示进度对话框，并在
                    // operationFinished 后重新列出内容。
                    if (!std::holds_alternative<std::monostate>(
                            m_writeAfterReopen)) {
                        auto write = std::move(m_writeAfterReopen);
                        m_writeAfterReopen = std::monostate{};
                        m_busy = true;
                        backend->clearCancel();
                        const bool invoked = std::visit(
                            Overloaded{
                                [this, backend](const AddFilesOptions &options) {
                                    m_pending = options;
                                    m_retryCount = 0;
                                    return QMetaObject::invokeMethod(
                                        backend,
                                        [backend, options]() {
                                            backend->addFiles(options);
                                        },
                                        Qt::QueuedConnection);
                                },
                                [this, backend](const DeleteFilesOptions &options) {
                                    m_pending = options;
                                    m_retryCount = 0;
                                    return QMetaObject::invokeMethod(
                                        backend,
                                        [backend, options]() {
                                            backend->deleteFiles(options);
                                        },
                                        Qt::QueuedConnection);
                                },
                                [](const std::monostate &) { return false; },
                            },
                            write);
                        if (!invoked) {
                            m_busy = false;
                            clearPendingState();
                            emit errorOccurred(
                                ArchiveErrorCode::Unknown,
                                tr("Failed to dispatch the archive operation"));
                        }
                        return;
                    }

                    RecentFilesManager::instance().add(filePath);
                    emit archiveOpened(filePath);
                    return;
                }

                m_opening = false;
                if (m_fallbackInProgress) {
                    // Re-opening on the CLI backend after a fallback: the UI
                    // already knows this archive, do not re-announce it.
                    m_fallbackInProgress = false;
                    return;
                }
                RecentFilesManager::instance().add(filePath);
                emit archiveOpened(filePath);
            });

    connect(backend, &ArchiveBackend::fileListReady, this,
            [this, isCurrent](const QList<ArchiveEntry> &entries) {
                if (isCurrent()) {
                    m_entries = entries;
                    m_busy = false;
                    m_pending = std::monostate{};
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
            clearPendingState();
            m_preTestExtractOptions = ExtractOptions{};
            emit operationFinished(false, msg);
            return;
        }

        ExtractOptions options = m_preTestExtractOptions;
        options.testBeforeExtract = false;
        startExtractOperation(options);
        return;
    }

    m_pending = std::monostate{};
    m_retryCount = 0;
    emit operationFinished(success, msg);
}

bool ArchiveManager::tryLibArchiveFallback(ArchiveBackend *backend,
                                           ArchiveErrorCode code, const QString &msg)
{
    if (backend != m_backend || !m_usingLibArchive || m_fallbackDone) {
        return false;
    }

    switch (code) {
    case ArchiveErrorCode::Cancelled:
    case ArchiveErrorCode::OperationNotSupported:
    case ArchiveErrorCode::PathTraversalBlocked:
    case ArchiveErrorCode::InsufficientDiskSpace:
    case ArchiveErrorCode::ToolNotFound:
    case ArchiveErrorCode::FileNotFound:
    case ArchiveErrorCode::PermissionDenied:
    case ArchiveErrorCode::PartialExtraction:
    case ArchiveErrorCode::HardLinkUnsupported:
        return false;
    default:
        break;
    }

    const bool retryable = std::holds_alternative<ListOptions>(m_pending)
        || std::holds_alternative<ExtractOptions>(m_pending)
        || std::holds_alternative<TestOptions>(m_pending);
    if (!retryable) {
        return false;
    }

    m_fallbackDone = true;
    m_usingLibArchive = false;

    LOG_INFO("ArchiveManager",
             "libarchive could not handle %s (%s); retrying with the CLI backend",
             m_archivePath.toUtf8().constData(), msg.toUtf8().constData());
    emit outputReady(
        tr("libarchive cannot handle this archive, switching to the 7z command line."));

    ArchiveBackend *old = m_backend;
    m_backend = nullptr;
    old->requestCancel();
    disconnect(old, nullptr, this, nullptr);
    QMetaObject::invokeMethod(
        old, [old]() { old->closeArchive(); delete old; }, Qt::QueuedConnection);

    ArchiveBackend *cli = ArchiveBackendFactory::create(m_format);
    if (!cli) {
        m_busy = false;
        clearPendingState();
        emit errorOccurred(code, msg);
        return true;
    }

    cli->moveToThread(&m_workerThread);
    m_backend = cli;
    m_fallbackInProgress = true;
    m_busy = true;

    const quint64 generation = m_generation;
    connectBackend(cli, generation);

    const QString path = m_archivePath;
    const auto pending = m_pending;
    // When the failing operation was the "test before extract" pre-test, that
    // pre-test must be replayed, not the extraction itself.
    const bool preTest = m_preTestPending;
    const QString preTestPassword = m_preTestExtractOptions.password;

    QMetaObject::invokeMethod(
        cli,
        [cli, path, pending, generation, preTest, preTestPassword, this]() {
            if (generation != m_generation) {
                return;
            }
            cli->openArchive(path);

            if (preTest) {
                TestOptions testOptions;
                testOptions.password = preTestPassword;
                cli->testArchive(testOptions);
                return;
            }

            std::visit(Overloaded{
                           [cli](const ListOptions &options) {
                               cli->listContents(options);
                           },
                           [cli](const ExtractOptions &options) {
                               cli->extract(options);
                           },
                           [cli](const TestOptions &options) {
                               cli->testArchive(options);
                           },
                           [](const auto &) {},
                       },
                       pending);
        },
        Qt::QueuedConnection);
    return true;
}

void ArchiveManager::handleError(ArchiveBackend *backend, ArchiveErrorCode code,
                                 const QString &msg)
{
    if (tryLibArchiveFallback(backend, code, msg)) {
        return;
    }

    m_busy = false;

    if (backend == m_openingBackend) {
        m_openingBackend = nullptr;
        m_opening = false;
        m_openingPath.clear();
        m_openingFormat = ArchiveFormat::Unknown;
        clearWriteAfterReopen();
        disconnect(backend, nullptr, this, nullptr);
        destroyBackend(backend);
        emit errorOccurred(code, msg);
        return;
    }

    if (backend == m_tempBackend) {
        removeFailedCompressOutput();
        emit errorOccurred(code, msg);
        cleanupTempBackend();
        cleanupCompressSession();
        return;
    }

    if (m_preTestPending) {
        m_preTestPending = false;
        m_preTestExtractOptions = ExtractOptions{};
        clearPendingState();
    }

    if (!m_opening) {
        clearPendingState();
    }

    emit errorOccurred(code, msg);
}
