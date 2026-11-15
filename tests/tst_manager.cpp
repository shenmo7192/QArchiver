#include <QtTest>

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "backends/tar/TarBackend.h"
#include "core/ArchiveManager.h"
#include "core/ArchiveOptions.h"

class TestManager : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void openListExtract();
    void compressOverwritesExistingTarget();
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

QTEST_GUILESS_MAIN(TestManager)

#include "tst_manager.moc"
