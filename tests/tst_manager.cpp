// QArchiver / 小Q解压
// Tests for ArchiveManager routing, retries and error handling.
// ArchiveManager 路由、重试与错误处理测试。

#include <QtTest>

#include <QDesktopServices>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include <memory>

#include "backends/archive/LibArchiveBackend.h"
#include "backends/rar/RarBackend.h"
#include "backends/sevenzip/SevenZipBackend.h"
#include "backends/tar/TarBackend.h"
#include "backends/zip/ZipBackend.h"
#include "core/AppSettings.h"
#include "core/ArchiveManager.h"
#include "core/ArchiveOptions.h"
#include "ui/PreviewManager.h"
#include "ui/PreviewUtils.h"
#include "utils/FileUtils.h"
#include "utils/ProcessRunner.h"

class PreviewUrlCapture : public QObject {
    Q_OBJECT
public:
    QList<QUrl> urls;

public slots:
    void handleUrl(const QUrl &url)
    {
        urls << url;
    }
};

class TestManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void openListExtract();
    void compressOverwritesExistingTarget();
    void encrypted7zFallsBackToCli();
    void rarSplitCompressionReplacesVolumes();
    void continuousPreviewExtractsSiblingImages();
    void openFailurePreservesCurrentArchive();
    void unsafeSymlinkRequiresDecision();
    void backendSelectionHonoursPreference();
};

// Prepare the manager test environment.
// 准备管理器测试环境。
void TestManager::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("QArchiverTest"));
    QCoreApplication::setApplicationName(QStringLiteral("QArchiverTest"));
}

// Open, list and extract through ArchiveManager.
// 通过 ArchiveManager 打开、列出并解压。
void TestManager::openListExtract()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = temp.filePath(QStringLiteral("payload.txt"));
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray payload = QByteArrayLiteral("manager-data");
        QCOMPARE(file.write(payload), qint64(payload.size()));
    }

    const QString archive = temp.filePath(QStringLiteral("manager.tar"));
    {
        TarBackend backend(ArchiveFormat::Tar);
        CompressOptions options;
        options.archivePath = archive;
        options.sourceFiles = {source};
        bool finished = false;
        bool success = false;
        connect(&backend, &ArchiveBackend::operationFinished, &backend,
                [&finished, &success](bool ok, const QString &) {
                    finished = true;
                    success = ok;
                });
        backend.compress(options);
        QVERIFY(finished);
        QVERIFY(success);
    }

    ArchiveManager &manager = ArchiveManager::instance();
    QSignalSpy openedSpy(&manager, &ArchiveManager::archiveOpened);
    QSignalSpy listSpy(&manager, &ArchiveManager::fileListReady);

    manager.openArchive(archive);
    QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 10000);
    QCOMPARE(manager.currentPath(), archive);
    QCOMPARE(manager.currentFormat(), ArchiveFormat::Tar);

    manager.listContents();
    QTRY_COMPARE_WITH_TIMEOUT(listSpy.count(), 1, 10000);
    QVERIFY(!manager.entries().isEmpty());

    const QString dest = temp.filePath(QStringLiteral("out"));
    QSignalSpy finishSpy(&manager, &ArchiveManager::operationFinished);
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    manager.extract(extractOptions);
    QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 10000);
    QVERIFY(finishSpy.first().at(0).toBool());

    QFile extracted(dest + QStringLiteral("/payload.txt"));
    QVERIFY(extracted.open(QIODevice::ReadOnly));
    QCOMPARE(extracted.readAll(), QByteArrayLiteral("manager-data"));

    manager.closeArchive();
}

// Compression replaces an existing target safely.
// 压缩操作安全替换已有目标文件。
void TestManager::compressOverwritesExistingTarget()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = temp.filePath(QStringLiteral("source.txt"));
    const QByteArray payload = QByteArrayLiteral("replacement-content");
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(payload), qint64(payload.size()));
    }

    const QString target = temp.filePath(QStringLiteral("target.tar"));
    {
        QFile file(target);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArrayLiteral("old-content")), qint64(11));
    }

    ArchiveManager &manager = ArchiveManager::instance();
    QSignalSpy finishSpy(&manager, &ArchiveManager::operationFinished);
    CompressOptions options;
    options.archivePath = target;
    options.sourceFiles = {source};
    manager.compress(options, ArchiveFormat::Tar);
    QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 10000);
    QVERIFY2(finishSpy.first().at(0).toBool(),
             qPrintable(finishSpy.first().at(1).toString()));

    QSignalSpy openedSpy(&manager, &ArchiveManager::archiveOpened);
    QSignalSpy listSpy(&manager, &ArchiveManager::fileListReady);
    manager.openArchive(target);
    QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 10000);
    manager.listContents();
    QTRY_COMPARE_WITH_TIMEOUT(listSpy.count(), 1, 10000);

    const QString dest = temp.filePath(QStringLiteral("out"));
    QSignalSpy extractSpy(&manager, &ArchiveManager::operationFinished);
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    manager.extract(extractOptions);
    QTRY_COMPARE_WITH_TIMEOUT(extractSpy.count(), 1, 10000);
    QVERIFY(extractSpy.first().at(0).toBool());

    QFile extracted(dest + QStringLiteral("/source.txt"));
    QVERIFY(extracted.open(QIODevice::ReadOnly));
    QCOMPARE(extracted.readAll(), payload);
    manager.closeArchive();
}

// An encrypted 7z cannot be read by libarchive. The manager must notice the
// UnsupportedFormat error, rebuild the backend on the 7z command line and
// replay the pending operation (including the password prompt).
// Encrypted 7z falls back from libarchive to CLI.
// 加密 7z 从 libarchive 回退到 CLI。
void TestManager::encrypted7zFallsBackToCli()
{
#ifdef QARCHIVER_HAS_LIBARCHIVE
    const QString sevenZip = ProcessRunner::resolveToolPath(QStringLiteral("7z"));
    if (sevenZip.isEmpty()) {
        QSKIP("the 7z tool is required for this test");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString payloadPath = temp.filePath(QStringLiteral("secret.txt"));
    {
        QFile file(payloadPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArrayLiteral("classified")), qint64(10));
    }

    const QString archive = temp.filePath(QStringLiteral("secret.7z"));
    {
        QProcess process;
        process.setWorkingDirectory(temp.path());
        process.start(sevenZip,
                      {QStringLiteral("a"), QStringLiteral("-bso0"),
                       QStringLiteral("-bsp0"), QStringLiteral("-pclassified"),
                       archive, payloadPath});
        QVERIFY(process.waitForFinished(60000));
        QCOMPARE(process.exitCode(), 0);
    }

    ArchiveManager &manager = ArchiveManager::instance();
    QSignalSpy openedSpy(&manager, &ArchiveManager::archiveOpened);
    QSignalSpy listSpy(&manager, &ArchiveManager::fileListReady);

    manager.openArchive(archive);
    QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 10000);

    // Listing works: 7z does not encrypt the entry names by default, so both
    // libarchive and the CLI can list the archive.
    manager.listContents();
    QTRY_COMPARE_WITH_TIMEOUT(listSpy.count(), 1, 10000);

    // Extraction is where libarchive gives up; the manager must rebuild the
    // backend on the 7z command line and replay the extraction.
    const QString dest = temp.filePath(QStringLiteral("out"));
    QSignalSpy finishSpy(&manager, &ArchiveManager::operationFinished);
    ExtractOptions options;
    options.destDir = dest;
    options.overwrite = OverwritePolicy::Always;
    options.password = QStringLiteral("classified");
    manager.extract(options);

    QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 20000);
    QVERIFY2(finishSpy.first().at(0).toBool(),
             qPrintable(finishSpy.first().at(1).toString()));
    QVERIFY(qobject_cast<SevenZipBackend *>(manager.backend()) != nullptr);

    QFile extracted(
        dest + QStringLiteral("/") + QFileInfo(payloadPath).fileName());
    QVERIFY(extracted.open(QIODevice::ReadOnly));
    QCOMPARE(extracted.readAll(), QByteArrayLiteral("classified"));

    manager.closeArchive();
#else
    QSKIP("built without libarchive");
#endif
}

// Creating RAR volumes through the manager replaces an older volume set.
// 通过管理器创建 RAR 分卷时，必须替换旧的分卷集。
void TestManager::rarSplitCompressionReplacesVolumes()
{
    if (!RarBackend::isCreateAvailable()) {
        QSKIP("the rar command is not installed");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = temp.filePath(QStringLiteral("payload.bin"));
    QByteArray payload;
    payload.reserve(12000);
    for (int i = 0; i < 12000; ++i) {
        payload.append(static_cast<char>((i * 17 + 3) & 0xff));
    }
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(payload), qint64(payload.size()));
    }

    const QString archive = temp.filePath(QStringLiteral("volumes.rar"));
    ArchiveManager &manager = ArchiveManager::instance();

    CompressOptions splitOptions;
    splitOptions.archivePath = archive;
    splitOptions.sourceFiles = {source};
    splitOptions.compressionLevel = 0;
    splitOptions.volumeSize = 3000;

    QSignalSpy splitSpy(&manager, &ArchiveManager::operationFinished);
    manager.compress(splitOptions, ArchiveFormat::Rar5);
    QTRY_COMPARE_WITH_TIMEOUT(splitSpy.count(), 1, 30000);
    QVERIFY2(splitSpy.first().at(0).toBool(),
             qPrintable(splitSpy.first().at(1).toString()));

    const QString firstVolume = temp.filePath(QStringLiteral("volumes.part1.rar"));
    const QString secondVolume = temp.filePath(QStringLiteral("volumes.part2.rar"));
    QVERIFY(QFileInfo::exists(firstVolume));
    QVERIFY(QFileInfo::exists(secondVolume));

    // Re-create the same target as a single-volume archive: the old part
    // files must be removed before the new archive is promoted.
    // 将同一目标重新创建为单卷压缩包：旧分卷必须在新包替换前删除。
    CompressOptions singleOptions = splitOptions;
    singleOptions.volumeSize = 0;
    QSignalSpy singleSpy(&manager, &ArchiveManager::operationFinished);
    manager.compress(singleOptions, ArchiveFormat::Rar5);
    QTRY_COMPARE_WITH_TIMEOUT(singleSpy.count(), 1, 30000);
    QVERIFY2(singleSpy.first().at(0).toBool(),
             qPrintable(singleSpy.first().at(1).toString()));

    QVERIFY(QFileInfo::exists(archive));
    const QStringList remaining = QDir(temp.path()).entryList(
        {QStringLiteral("volumes.part*.rar")}, QDir::Files);
    QVERIFY2(remaining.isEmpty(), qPrintable(remaining.join(QStringLiteral(", "))));

    QSignalSpy openSpy(&manager, &ArchiveManager::archiveOpened);
    QSignalSpy listSpy(&manager, &ArchiveManager::fileListReady);
    manager.openArchive(archive);
    QTRY_COMPARE_WITH_TIMEOUT(openSpy.count(), 1, 10000);
    manager.listContents();
    QTRY_COMPARE_WITH_TIMEOUT(listSpy.count(), 1, 10000);
    QCOMPARE(manager.currentFormat(), ArchiveFormat::Rar5);
    QVERIFY(!manager.entries().isEmpty());

    manager.closeArchive();
}

// The "use libarchive" preference decides which backend serves a plain archive;
// split volumes and rar always stay on the command line.
// Opening one image must prepare all same-directory images for the system
// viewer.  A custom file URL handler stands in for the external viewer.
// 打开一张图片时必须准备好同目录所有图片，供系统查看器连续切换。
// 使用自定义 file URL 处理器代替外部查看器。
void TestManager::continuousPreviewExtractsSiblingImages()
{
    const QString fixture = QStringLiteral(QARCHIVER_TEST_DATA_DIR)
        + QStringLiteral("/连续看图测试.tar.gz");
    if (!QFileInfo::exists(fixture)) {
        QSKIP("continuous preview fixture is missing");
    }
    if (ProcessRunner::resolveToolPath(QStringLiteral("7z")).isEmpty()) {
        QSKIP("the 7z tool is required for the preview fixture");
    }

    ArchiveManager &manager = ArchiveManager::instance();
    QSignalSpy openedSpy(&manager, &ArchiveManager::archiveOpened);
    QSignalSpy listSpy(&manager, &ArchiveManager::fileListReady);
    manager.openArchive(fixture);
    QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 10000);
    manager.listContents();
    QTRY_COMPARE_WITH_TIMEOUT(listSpy.count(), 1, 10000);

    const QString target = QStringLiteral("连续看图测试/1.png");
    qint64 targetSize = 0;
    for (const ArchiveEntry &entry : manager.entries()) {
        if (entry.path == target) {
            targetSize = entry.size;
            break;
        }
    }
    QVERIFY(targetSize > 0);

    const QStringList images = PreviewUtils::imageSiblings(manager.entries(), target);
    QCOMPARE(images.size(), 5);

    PreviewUrlCapture capture;
    QDesktopServices::setUrlHandler(QStringLiteral("file"), &capture, "handleUrl");
    PreviewManager::instance().cleanupOwn();
    PreviewManager::instance().preview(manager.currentPath(),
                                       manager.currentFormat(), target,
                                       targetSize, images, QString());

    QTRY_COMPARE_WITH_TIMEOUT(capture.urls.size(), 1, 15000);
    QDesktopServices::unsetUrlHandler(QStringLiteral("file"));

    const QString opened = capture.urls.first().toLocalFile();
    QVERIFY2(QFileInfo::exists(opened), qPrintable(opened));
    const QString previewRoot = QFileInfo(opened).absolutePath();
    QVERIFY(previewRoot.endsWith(QStringLiteral("连续看图测试")));

    for (const QString &image : images) {
        const QString extracted = QDir(previewRoot).filePath(
            QFileInfo(FileUtils::normalizeEntryPath(image)).fileName());
        QVERIFY2(QFileInfo::exists(extracted), qPrintable(extracted));
    }
    QVERIFY(!QFileInfo::exists(QDir(previewRoot).filePath(
        QStringLiteral("表情、微笑.svg"))));

    PreviewManager::instance().cleanupOwn();
    manager.closeArchive();
}

// A failed open must leave the currently open archive intact.
// 打开失败时必须保留当前已打开的压缩包。
void TestManager::openFailurePreservesCurrentArchive()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath(QStringLiteral("current.txt"));
    {
        QFile file(source);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArrayLiteral("current")), qint64(7));
    }
    const QString archive = temp.filePath(QStringLiteral("current.tar"));
    {
        TarBackend backend(ArchiveFormat::Tar);
        CompressOptions options;
        options.archivePath = archive;
        options.sourceFiles = {source};
        bool finished = false;
        connect(&backend, &ArchiveBackend::operationFinished, &backend,
                [&finished](bool, const QString &) { finished = true; });
        backend.compress(options);
        QVERIFY(finished);
    }

    ArchiveManager &manager = ArchiveManager::instance();
    manager.closeArchive();
    QSignalSpy openedSpy(&manager, &ArchiveManager::archiveOpened);
    manager.openArchive(archive);
    QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 10000);

    QSignalSpy errorSpy(&manager, &ArchiveManager::errorOccurred);
    manager.openArchive(temp.filePath(QStringLiteral("missing.tar")));
    QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 10000);
    QCOMPARE(errorSpy.first().at(0).value<ArchiveErrorCode>(),
             ArchiveErrorCode::FileNotFound);
    QCOMPARE(manager.currentPath(), archive);
    QVERIFY(manager.backend() != nullptr);

    manager.closeArchive();
}

// Unsafe symlinks are detected before the CLI extractor runs; the user can
// skip them, or abort without creating the destination.
// 在命令行解压器运行前检测危险符号链接；用户可跳过，或中止且不创建目标目录。
void TestManager::unsafeSymlinkRequiresDecision()
{
    const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (tar.isEmpty()) {
        QSKIP("the tar tool is required for this test");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString src = temp.filePath(QStringLiteral("src"));
    QVERIFY(QDir().mkpath(src));
    {
        QFile file(QDir(src).filePath(QStringLiteral("safe.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(QByteArrayLiteral("safe")), qint64(4));
    }
    QVERIFY(QFile::link(QStringLiteral("/etc/passwd"),
                        QDir(src).filePath(QStringLiteral("escape"))));

    const QString archive = temp.filePath(QStringLiteral("unsafe.tar"));
    {
        QProcess process;
        process.start(tar, {QStringLiteral("-cf"), archive,
                            QStringLiteral("-C"), src, QStringLiteral(".")});
        QVERIFY(process.waitForFinished(30000));
        QCOMPARE(process.exitCode(), 0);
    }

    ArchiveManager &manager = ArchiveManager::instance();
    manager.closeArchive();
    QSignalSpy openedSpy(&manager, &ArchiveManager::archiveOpened);
    QSignalSpy listSpy(&manager, &ArchiveManager::fileListReady);
    manager.openArchive(archive);
    QTRY_COMPARE_WITH_TIMEOUT(openedSpy.count(), 1, 10000);
    manager.listContents();
    QTRY_COMPARE_WITH_TIMEOUT(listSpy.count(), 1, 10000);

    QString linkPath;
    for (const ArchiveEntry &entry : manager.entries()) {
        if (entry.isSymLink) {
            linkPath = entry.path;
            break;
        }
    }
    QVERIFY2(!linkPath.isEmpty(), "tar did not expose the symlink entry");

    int decisions = 0;
    manager.setSymlinkDecisionCallback(
        [&decisions](const QString &, const QString &) {
            ++decisions;
            return ArchiveManager::SymlinkDecision::SkipAll;
        });

    const QString dest = temp.filePath(QStringLiteral("out-skip"));
    QSignalSpy finishSpy(&manager, &ArchiveManager::operationFinished);
    ExtractOptions options;
    options.destDir = dest;
    options.overwrite = OverwritePolicy::Always;
    manager.extract(options);
    QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 20000);
    QVERIFY2(finishSpy.first().at(0).toBool(),
             qPrintable(finishSpy.first().at(1).toString()));
    QCOMPARE(decisions, 1);
    QVERIFY(QFileInfo::exists(QDir(dest).filePath(QStringLiteral("safe.txt"))));
    QVERIFY(!QFileInfo::exists(QDir(dest).filePath(linkPath)));

    manager.setSymlinkDecisionCallback(
        [](const QString &, const QString &) {
            return ArchiveManager::SymlinkDecision::Abort;
        });
    const QString abortDest = temp.filePath(QStringLiteral("out-abort"));
    QSignalSpy errorSpy(&manager, &ArchiveManager::errorOccurred);
    ExtractOptions abortOptions;
    abortOptions.destDir = abortDest;
    abortOptions.overwrite = OverwritePolicy::Always;
    manager.extract(abortOptions);
    QTRY_COMPARE_WITH_TIMEOUT(errorSpy.count(), 1, 10000);
    QCOMPARE(errorSpy.first().at(0).value<ArchiveErrorCode>(),
             ArchiveErrorCode::Cancelled);
    QVERIFY(!QFileInfo::exists(abortDest));

    manager.setSymlinkDecisionCallback(nullptr);
    manager.closeArchive();
}

// Backend selection follows preference and split-volume rules.
// 后端选择遵循偏好设置与分卷规则。
void TestManager::backendSelectionHonoursPreference()
{
#ifdef QARCHIVER_HAS_LIBARCHIVE
    QVERIFY(ArchiveManager::isLibArchiveAvailable());

    AppSettings &settings = AppSettings::instance();
    const bool saved = settings.useLibArchiveBackend();

    settings.setUseLibArchiveBackend(false);
    QVERIFY(!settings.useLibArchiveBackend());
    {
        std::unique_ptr<ArchiveBackend> backend(ArchiveManager::createReadBackend(
            ArchiveFormat::Zip, QStringLiteral("archive.zip")));
        QVERIFY(qobject_cast<LibArchiveBackend *>(backend.get()) == nullptr);
        QVERIFY(qobject_cast<ZipBackend *>(backend.get()) != nullptr);
    }
    {
        std::unique_ptr<ArchiveBackend> backend(ArchiveManager::createReadBackend(
            ArchiveFormat::TarGz, QStringLiteral("archive.tar.gz")));
        QVERIFY(qobject_cast<LibArchiveBackend *>(backend.get()) == nullptr);
        QVERIFY(qobject_cast<TarBackend *>(backend.get()) != nullptr);
    }

    settings.setUseLibArchiveBackend(true);
    QVERIFY(settings.useLibArchiveBackend());
    {
        std::unique_ptr<ArchiveBackend> backend(ArchiveManager::createReadBackend(
            ArchiveFormat::Zip, QStringLiteral("archive.zip")));
        QVERIFY(qobject_cast<LibArchiveBackend *>(backend.get()) != nullptr);
    }
    {
        std::unique_ptr<ArchiveBackend> backend(ArchiveManager::createReadBackend(
            ArchiveFormat::TarGz, QStringLiteral("archive.tar.gz")));
        QVERIFY(qobject_cast<LibArchiveBackend *>(backend.get()) != nullptr);
    }

    // Not candidates for the libarchive backend, even when enabled: split
    // volumes have to be resolved by the CLI, and RAR stays on RarBackend.
    {
        std::unique_ptr<ArchiveBackend> backend(ArchiveManager::createReadBackend(
            ArchiveFormat::SevenZip, QStringLiteral("archive.7z.001")));
        QVERIFY(qobject_cast<SevenZipBackend *>(backend.get()) != nullptr);
    }
    {
        std::unique_ptr<ArchiveBackend> backend(ArchiveManager::createReadBackend(
            ArchiveFormat::Rar5, QStringLiteral("archive.rar")));
        QVERIFY(qobject_cast<LibArchiveBackend *>(backend.get()) == nullptr);
    }

    // The troubleshooting environment variable overrides the preference.
    qputenv("QARCHIVER_NO_LIBARCHIVE", "1");
    QVERIFY(ArchiveManager::isLibArchiveDisabledByEnvironment());
    {
        std::unique_ptr<ArchiveBackend> backend(ArchiveManager::createReadBackend(
            ArchiveFormat::Zip, QStringLiteral("archive.zip")));
        QVERIFY(qobject_cast<LibArchiveBackend *>(backend.get()) == nullptr);
        QVERIFY(qobject_cast<ZipBackend *>(backend.get()) != nullptr);
    }
    qunsetenv("QARCHIVER_NO_LIBARCHIVE");
    QVERIFY(!ArchiveManager::isLibArchiveDisabledByEnvironment());

    settings.setUseLibArchiveBackend(saved);
#else
    QVERIFY(!ArchiveManager::isLibArchiveAvailable());
    std::unique_ptr<ArchiveBackend> backend(ArchiveManager::createReadBackend(
        ArchiveFormat::Zip, QStringLiteral("archive.zip")));
    QVERIFY(qobject_cast<ZipBackend *>(backend.get()) != nullptr);
#endif
}

QTEST_GUILESS_MAIN(TestManager)

#include "tst_manager.moc"
