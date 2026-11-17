#include <QtTest>

#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <memory>

#include "backends/archive/LibArchiveBackend.h"
#include "backends/sevenzip/SevenZipBackend.h"
#include "backends/tar/TarBackend.h"
#include "backends/zip/ZipBackend.h"
#include "core/AppSettings.h"
#include "core/ArchiveManager.h"
#include "core/ArchiveOptions.h"
#include "utils/ProcessRunner.h"

class TestManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void openListExtract();
    void compressOverwritesExistingTarget();
    void encrypted7zFallsBackToCli();
    void backendSelectionHonoursPreference();
};

void TestManager::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("QArchiverTest"));
    QCoreApplication::setApplicationName(QStringLiteral("QArchiverTest"));
}

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

// The "use libarchive" preference decides which backend serves a plain archive;
// split volumes and rar always stay on the command line.
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
    // volumes have to be resolved by the CLI, and rar keeps using unrar.
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
