#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/ArchiveOptions.h"
#include "utils/FileUtils.h"
#include "utils/ProcessRunner.h"

#include "ui/DragOutManager.h"

// Exercises the "prepare files for dragging" staging logic: path pruning,
// staging/mapping, disk checks and the CLI fallback for archives libarchive
// cannot read.  No widgets are involved, so the test runs headless.
class TestDragOut : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void pruneNestedPathsKeepsTopLevel();
    void selectedUncompressedSizeSumsSelection();
    void mapStagedPathsFindsNestedFile();
    void mapStagedPathsStreamFallback();

    void stagesSelectedFile();
    void stagesDirectorySubtree();
    void prunesNestedSelection();
    void blocksUnsafeSelection();
    void stagesPlainGzipStream();
    void reusesPreparedStaging();
    void cacheMissAfterPasswordChange();
    void encrypted7zFallsBackToCli();

    void worksWithCommandLineBackend();

private:
    static bool writeFile(const QString &path, const QByteArray &data)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(data) == data.size();
    }

    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    static bool runTool(const QString &program, const QStringList &arguments,
                        const QString &workDir = QString())
    {
        QProcess process;
        if (!workDir.isEmpty()) {
            process.setWorkingDirectory(workDir);
        }
        process.start(program, arguments);
        if (!process.waitForFinished(120000)) {
            return false;
        }
        return process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0;
    }

    QString createZip(const QString &name, const QStringList &extraArgs = {},
                      const QString &password = QString());
    QString createEncrypted7z(const QString &name, const QString &password);

    QTemporaryDir m_tempDir;
    QString m_7z;
    QString m_sourceDir;
};

void TestDragOut::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setOrganizationName(QStringLiteral("QArchiverTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_dragout"));

    QVERIFY(m_tempDir.isValid());

    m_7z = ProcessRunner::resolveToolPath(QStringLiteral("7z"));
    if (m_7z.isEmpty()) {
        QSKIP("the 7z tool is required to build the test archives");
    }

    m_sourceDir = QDir(m_tempDir.path()).filePath(QStringLiteral("src"));
    QVERIFY(writeFile(QDir(m_sourceDir).filePath(QStringLiteral("root.txt")),
                      QByteArrayLiteral("root")));
    QVERIFY(writeFile(QDir(m_sourceDir).filePath(QStringLiteral("sub/a.txt")),
                      QByteArrayLiteral("alpha")));
    QVERIFY(writeFile(QDir(m_sourceDir).filePath(QStringLiteral("sub/deep/b.txt")),
                      QByteArrayLiteral("bravo")));
}

void TestDragOut::cleanupTestCase()
{
    DragOutManager::instance().cleanupOwn();
}

QString TestDragOut::createZip(const QString &name, const QStringList &extraArgs,
                               const QString &password)
{
    const QString archivePath = QDir(m_tempDir.path()).filePath(name);
    QStringList args = {QStringLiteral("a"), QStringLiteral("-tzip"),
                        QStringLiteral("-bso0"), QStringLiteral("-bsp0")};
    args << extraArgs;
    if (!password.isEmpty()) {
        args << QStringLiteral("-p%1").arg(password);
    }
    args << archivePath << QStringLiteral("root.txt") << QStringLiteral("sub");
    return runTool(m_7z, args, m_sourceDir) ? archivePath : QString();
}

QString TestDragOut::createEncrypted7z(const QString &name,
                                       const QString &password)
{
    const QString archivePath = QDir(m_tempDir.path()).filePath(name);
    const QStringList args = {QStringLiteral("a"), QStringLiteral("-t7z"),
                              QStringLiteral("-bso0"), QStringLiteral("-bsp0"),
                              QStringLiteral("-p%1").arg(password),
                              archivePath, QStringLiteral("root.txt"),
                              QStringLiteral("sub")};
    return runTool(m_7z, args, m_sourceDir) ? archivePath : QString();
}

void TestDragOut::pruneNestedPathsKeepsTopLevel()
{
    const QStringList pruned = DragOutManager::pruneNestedPaths(
        {QStringLiteral("sub"), QStringLiteral("sub/a.txt"),
         QStringLiteral("root.txt"), QStringLiteral("sub/deep")});

    QCOMPARE(pruned.size(), 2);
    QVERIFY(pruned.contains(QStringLiteral("sub")));
    QVERIFY(pruned.contains(QStringLiteral("root.txt")));
}

void TestDragOut::selectedUncompressedSizeSumsSelection()
{
    QList<ArchiveEntry> entries;
    auto add = [&entries](const QString &path, qint64 size, bool isDir = false) {
        ArchiveEntry entry;
        entry.path = path;
        entry.size = size;
        entry.isDir = isDir;
        entries.append(entry);
    };
    add(QStringLiteral("root.txt"), 100);
    add(QStringLiteral("sub"), 0, true);
    add(QStringLiteral("sub/a.txt"), 200);
    add(QStringLiteral("sub/deep/b.txt"), 300);
    add(QStringLiteral("other/c.txt"), 400);

    QCOMPARE(DragOutManager::selectedUncompressedSize(
                 entries, {QStringLiteral("root.txt")}),
             100);
    // Selecting a directory counts the whole subtree.
    QCOMPARE(DragOutManager::selectedUncompressedSize(
                 entries, {QStringLiteral("sub")}),
             500);
    // Overlapping selections are not counted twice.
    QCOMPARE(DragOutManager::selectedUncompressedSize(
                 entries, {QStringLiteral("sub"), QStringLiteral("sub/a.txt")}),
             500);
}

void TestDragOut::mapStagedPathsFindsNestedFile()
{
    QTemporaryDir staging;
    QVERIFY(staging.isValid());
    QVERIFY(writeFile(QDir(staging.path()).filePath(QStringLiteral("sub/a.txt")),
                      QByteArrayLiteral("alpha")));

    const QStringList mapped = DragOutManager::mapStagedPaths(
        staging.path(), {QStringLiteral("sub/a.txt")});
    QCOMPARE(mapped.size(), 1);
    QCOMPARE(mapped.first(),
             QFileInfo(QDir(staging.path()).filePath(QStringLiteral("sub/a.txt")))
                 .absoluteFilePath());

    // A path the caller did not select maps to nothing.
    QVERIFY(DragOutManager::mapStagedPaths(staging.path(),
                                           {QStringLiteral("missing.txt")})
                .isEmpty());
}

void TestDragOut::mapStagedPathsStreamFallback()
{
    QTemporaryDir staging;
    QVERIFY(staging.isValid());
    QVERIFY(writeFile(QDir(staging.path()).filePath(QStringLiteral("data")),
                      QByteArrayLiteral("payload")));

    // Plain streams are extracted under a synthetic name and the selection is
    // ignored, so the top-level staging entry is used instead.
    const QStringList mapped = DragOutManager::topLevelStagedPaths(staging.path());
    QCOMPARE(mapped.size(), 1);
    QCOMPARE(QFileInfo(mapped.first()).fileName(), QStringLiteral("data"));
}

void TestDragOut::stagesSelectedFile()
{
    const QString zip = createZip(QStringLiteral("plain.zip"));
    QVERIFY(!zip.isEmpty());

    auto &manager = DragOutManager::instance();
    const DragOutPayload payload = manager.extractForDrag(
        zip, ArchiveFormat::Zip, {}, {QStringLiteral("sub/a.txt")}, QString());

    QVERIFY2(payload.success, qPrintable(payload.errorMessage));
    QCOMPARE(payload.localPaths.size(), 1);
    QCOMPARE(readFile(payload.localPaths.first()), QByteArrayLiteral("alpha"));
    QVERIFY(payload.localPaths.first().startsWith(payload.stagingRoot));

    manager.retire(payload);
}

void TestDragOut::stagesDirectorySubtree()
{
    const QString zip = createZip(QStringLiteral("plain2.zip"));
    QVERIFY(!zip.isEmpty());

    auto &manager = DragOutManager::instance();
    const DragOutPayload payload = manager.extractForDrag(
        zip, ArchiveFormat::Zip, {}, {QStringLiteral("sub")}, QString());

    QVERIFY2(payload.success, qPrintable(payload.errorMessage));
    QCOMPARE(payload.localPaths.size(), 1);

    const QString stagedDir = payload.localPaths.first();
    QVERIFY(QFileInfo(stagedDir).isDir());
    QVERIFY(QFileInfo::exists(QDir(stagedDir).filePath(QStringLiteral("a.txt"))));
    QVERIFY(QFileInfo::exists(
        QDir(stagedDir).filePath(QStringLiteral("deep/b.txt"))));

    manager.retire(payload);
}

void TestDragOut::prunesNestedSelection()
{
    const QString zip = createZip(QStringLiteral("plain3.zip"));
    QVERIFY(!zip.isEmpty());

    auto &manager = DragOutManager::instance();
    const DragOutPayload payload = manager.extractForDrag(
        zip, ArchiveFormat::Zip, {},
        {QStringLiteral("sub"), QStringLiteral("sub/a.txt")}, QString());

    QVERIFY2(payload.success, qPrintable(payload.errorMessage));
    // The directory covers its children, so only one path is handed out.
    QCOMPARE(payload.localPaths.size(), 1);
    QVERIFY(QFileInfo(payload.localPaths.first()).isDir());

    manager.retire(payload);
}

void TestDragOut::blocksUnsafeSelection()
{
    const QString zip = createZip(QStringLiteral("plain4.zip"));
    QVERIFY(!zip.isEmpty());

    const DragOutPayload payload = DragOutManager::instance().extractForDrag(
        zip, ArchiveFormat::Zip, {},
        {QStringLiteral("../escape.txt")}, QString());

    QVERIFY(!payload.success);
    QCOMPARE(payload.errorCode, ArchiveErrorCode::PathTraversalBlocked);
    QVERIFY(payload.stagingRoot.isEmpty());
}

void TestDragOut::stagesPlainGzipStream()
{
    const QString archive = QDir(m_tempDir.path()).filePath(
        QStringLiteral("stream.gz"));
    QVERIFY(runTool(m_7z,
                    {QStringLiteral("a"), QStringLiteral("-tgzip"),
                     QStringLiteral("-bso0"), QStringLiteral("-bsp0"),
                     archive, QStringLiteral("root.txt")},
                    m_sourceDir));

    auto &manager = DragOutManager::instance();
    const DragOutPayload payload = manager.extractForDrag(
        archive, ArchiveFormat::Gzip, {}, {QStringLiteral("root.txt")},
        QString());

    QVERIFY2(payload.success, qPrintable(payload.errorMessage));
    QCOMPARE(payload.localPaths.size(), 1);
    QCOMPARE(readFile(payload.localPaths.first()), QByteArrayLiteral("root"));

    manager.retire(payload);
}

void TestDragOut::reusesPreparedStaging()
{
    const QString zip = createZip(QStringLiteral("cache.zip"));
    QVERIFY(!zip.isEmpty());

    auto &manager = DragOutManager::instance();
    const QStringList paths = {QStringLiteral("sub/a.txt")};
    const DragOutPayload first = manager.extractForDrag(
        zip, ArchiveFormat::Zip, {}, paths, QString());
    QVERIFY2(first.success, qPrintable(first.errorMessage));
    QVERIFY(manager.hasPreparedFor(zip, ArchiveFormat::Zip, paths, QString()));

    // The second drag of the same selection is served from the cache: no new
    // staging directory, identical paths.
    const DragOutPayload second = manager.extractForDrag(
        zip, ArchiveFormat::Zip, {}, paths, QString());
    QVERIFY(second.success);
    QCOMPARE(second.stagingRoot, first.stagingRoot);
    QCOMPARE(second.localPaths, first.localPaths);

    // A different password is a different preparation.
    QVERIFY(!manager.hasPreparedFor(zip, ArchiveFormat::Zip, paths,
                                    QStringLiteral("other")));
}

void TestDragOut::cacheMissAfterPasswordChange()
{
    const QString zip = createZip(QStringLiteral("cache2.zip"));
    QVERIFY(!zip.isEmpty());

    auto &manager = DragOutManager::instance();
    const QStringList paths = {QStringLiteral("root.txt")};
    const DragOutPayload payload = manager.extractForDrag(
        zip, ArchiveFormat::Zip, {}, paths, QString());
    QVERIFY2(payload.success, qPrintable(payload.errorMessage));

    QVERIFY(manager.hasPreparedFor(zip, ArchiveFormat::Zip, paths, QString()));
    QVERIFY(!manager.hasPreparedFor(zip, ArchiveFormat::Zip, paths,
                                    QStringLiteral("secret")));
    QVERIFY(!manager.hasPreparedFor(zip, ArchiveFormat::Zip,
                                    {QStringLiteral("root.txt"),
                                     QStringLiteral("sub/a.txt")},
                                    QString()));

    // Closing the archive drops the cache.
    manager.clearCache();
    QVERIFY(!manager.hasPreparedFor(zip, ArchiveFormat::Zip, paths, QString()));
}

void TestDragOut::encrypted7zFallsBackToCli()
{
    const QString archive = createEncrypted7z(QStringLiteral("enc.7z"),
                                              QStringLiteral("secret"));
    QVERIFY(!archive.isEmpty());

    auto &manager = DragOutManager::instance();
    const DragOutPayload payload = manager.extractForDrag(
        archive, ArchiveFormat::SevenZip, {}, {QStringLiteral("root.txt")},
        QStringLiteral("secret"));

    // libarchive cannot read encrypted 7z; the manager retries with the 7z
    // command line tool, which makes this succeed.
    QVERIFY2(payload.success, qPrintable(payload.errorMessage));
    QCOMPARE(payload.localPaths.size(), 1);
    QCOMPARE(readFile(payload.localPaths.first()), QByteArrayLiteral("root"));

    manager.retire(payload);
}

void TestDragOut::worksWithCommandLineBackend()
{
    const QString zip = createZip(QStringLiteral("plain5.zip"));
    QVERIFY(!zip.isEmpty());

    qputenv("QARCHIVER_NO_LIBARCHIVE", "1");
    auto &manager = DragOutManager::instance();
    const DragOutPayload payload = manager.extractForDrag(
        zip, ArchiveFormat::Zip, {}, {QStringLiteral("root.txt")}, QString());
    qunsetenv("QARCHIVER_NO_LIBARCHIVE");

    QVERIFY2(payload.success, qPrintable(payload.errorMessage));
    QCOMPARE(payload.localPaths.size(), 1);
    QCOMPARE(readFile(payload.localPaths.first()), QByteArrayLiteral("root"));

    manager.retire(payload);
}

QTEST_GUILESS_MAIN(TestDragOut)
#include "tst_dragout.moc"
