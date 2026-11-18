// QArchiver / 小Q解压
// Tests for the optional in-process libarchive backend.
// 可选进程内 libarchive 后端的测试。

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

#include "backends/archive/LibArchiveBackend.h"
#include "core/ArchiveOptions.h"
#include "utils/FileUtils.h"
#include "utils/ProcessRunner.h"

// Exercises the read-only libarchive backend against archives produced by the
// 7z/tar command line, i.e. exactly the archives QArchiver has to consume.
class TestLibArchive : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    void zipRoundTrip();
    void tarGzRoundTrip();
    void tarXzRoundTrip();
    void gzipStreamName();
    void aesZipNeedsPassword();
    void encrypted7zIsNotSupported();
    void corruptedArchiveIsDetected();
    void selectedExtractionOnly();
    void directorySelectionExtractsSubtree();

private:
    struct Result {
        bool finished = false;
        bool success = false;
        bool passwordRequested = false;
        ArchiveErrorCode code = ArchiveErrorCode::Unknown;
        QString message;
    };

    static QByteArray readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return file.readAll();
    }

    static bool writeFile(const QString &path, const QByteArray &data)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(data) == data.size();
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
        return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    }

    // Creates <root>/src/{a.txt,sub/b.txt} and returns root/src.
    static QString createSourceTree(const QString &root)
    {
        const QString srcDir = QDir(root).filePath(QStringLiteral("src"));
        writeFile(QDir(srcDir).filePath(QStringLiteral("a.txt")),
                  QByteArrayLiteral("alpha"));
        writeFile(QDir(srcDir).filePath(QStringLiteral("sub/b.txt")),
                  QByteArrayLiteral("bravo"));
        return srcDir;
    }

    bool makeArchive(const QString &root, const QStringList &arguments,
                     const QString &archivePath)
    {
        QStringList args = {QStringLiteral("a"), QStringLiteral("-bso0"),
                            QStringLiteral("-bsp0")};
        args << arguments << archivePath << createSourceTree(root);
        return runTool(m_7z, args);
    }

    static Result list(LibArchiveBackend &backend, const ListOptions &options,
                       QList<ArchiveEntry> *entries)
    {
        Result result;
        const auto listConnection = QObject::connect(
            &backend, &ArchiveBackend::fileListReady, &backend,
            [&result, entries](const QList<ArchiveEntry> &items) {
                result.finished = true;
                result.success = true;
                if (entries) {
                    *entries = items;
                }
            });
        const auto passwordConnection = QObject::connect(
            &backend, &ArchiveBackend::passwordRequested, &backend,
            [&result]() { result.passwordRequested = true; });
        const auto errorConnection = QObject::connect(
            &backend, &ArchiveBackend::errorOccurred, &backend,
            [&result](ArchiveErrorCode code, const QString &message) {
                result.finished = true;
                result.success = false;
                result.code = code;
                result.message = message;
            });
        backend.listContents(options);
        QObject::disconnect(listConnection);
        QObject::disconnect(passwordConnection);
        QObject::disconnect(errorConnection);
        return result;
    }

    static Result extract(LibArchiveBackend &backend,
                          const ExtractOptions &options)
    {
        Result result;
        const auto finishedConnection = QObject::connect(
            &backend, &ArchiveBackend::operationFinished, &backend,
            [&result](bool ok, const QString &message) {
                result.finished = true;
                result.success = ok;
                result.message = message;
            });
        const auto passwordConnection = QObject::connect(
            &backend, &ArchiveBackend::passwordRequested, &backend,
            [&result]() { result.passwordRequested = true; });
        const auto errorConnection = QObject::connect(
            &backend, &ArchiveBackend::errorOccurred, &backend,
            [&result](ArchiveErrorCode code, const QString &message) {
                result.finished = true;
                result.success = false;
                result.code = code;
                result.message = message;
            });
        backend.extract(options);
        QObject::disconnect(finishedConnection);
        QObject::disconnect(passwordConnection);
        QObject::disconnect(errorConnection);
        return result;
    }

    static Result test(LibArchiveBackend &backend, const TestOptions &options)
    {
        Result result;
        const auto finishedConnection = QObject::connect(
            &backend, &ArchiveBackend::operationFinished, &backend,
            [&result](bool ok, const QString &message) {
                result.finished = true;
                result.success = ok;
                result.message = message;
            });
        const auto errorConnection = QObject::connect(
            &backend, &ArchiveBackend::errorOccurred, &backend,
            [&result](ArchiveErrorCode code, const QString &message) {
                result.finished = true;
                result.success = false;
                result.code = code;
                result.message = message;
            });
        backend.testArchive(options);
        QObject::disconnect(finishedConnection);
        QObject::disconnect(errorConnection);
        return result;
    }

    static bool containsPath(const QList<ArchiveEntry> &entries, const QString &path)
    {
        for (const ArchiveEntry &entry : entries) {
            if (entry.path == path) {
                return true;
            }
        }
        return false;
    }

    QString m_7z;
    QString m_tar;
};

// Build the archive fixtures used by the tests.
// 构建测试使用的压缩包夹具。
void TestLibArchive::initTestCase()
{
    m_7z = ProcessRunner::resolveToolPath(QStringLiteral("7z"));
    if (m_7z.isEmpty()) {
        QSKIP("the 7z tool is required to build the test archives");
    }
    m_tar = ProcessRunner::resolveToolPath(QStringLiteral("tar"));
}

// List, extract and test zip through libarchive.
// 通过 libarchive 列出、解压并测试 zip。
void TestLibArchive::zipRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("test.zip"));
    QVERIFY2(makeArchive(temp.path(), {QStringLiteral("-tzip")}, archive),
             "failed to create the zip fixture");

    LibArchiveBackend backend(ArchiveFormat::Zip);
    backend.openArchive(archive);

    QList<ArchiveEntry> entries;
    const Result listed = list(backend, {}, &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    QVERIFY(containsPath(entries, QStringLiteral("src/a.txt")));
    QVERIFY(containsPath(entries, QStringLiteral("src/sub/b.txt")));

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("src/a.txt"))),
             QByteArrayLiteral("alpha"));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("src/sub/b.txt"))),
             QByteArrayLiteral("bravo"));

    const Result tested = test(backend, {});
    QVERIFY2(tested.success, qPrintable(tested.message));
}

// Read a gzip-compressed tar through libarchive.
// 通过 libarchive 读取 gzip 压缩的 tar。
void TestLibArchive::tarGzRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("test.tar.gz"));
    createSourceTree(temp.path());
    QVERIFY2(!m_tar.isEmpty()
                 && runTool(m_tar, {QStringLiteral("czf"), archive,
                                    QStringLiteral("-C"), temp.path(),
                                    QStringLiteral("src")}),
             "failed to create the tar.gz fixture");

    LibArchiveBackend backend(ArchiveFormat::TarGz);
    backend.openArchive(archive);

    QList<ArchiveEntry> entries;
    const Result listed = list(backend, {}, &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    QVERIFY(containsPath(entries, QStringLiteral("src/a.txt")));

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("src/sub/b.txt"))),
             QByteArrayLiteral("bravo"));
}

// Read an xz-compressed tar through libarchive.
// 通过 libarchive 读取 xz 压缩的 tar。
void TestLibArchive::tarXzRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("test.tar.xz"));
    createSourceTree(temp.path());
    QVERIFY2(!m_tar.isEmpty()
                 && runTool(m_tar, {QStringLiteral("cJf"), archive,
                                    QStringLiteral("-C"), temp.path(),
                                    QStringLiteral("src")}),
             "failed to create the tar.xz fixture");

    LibArchiveBackend backend(ArchiveFormat::TarXz);
    backend.openArchive(archive);

    QList<ArchiveEntry> entries;
    const Result listed = list(backend, {}, &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    QVERIFY(containsPath(entries, QStringLiteral("src/a.txt")));
}

// Infer the output name for a pure gzip stream.
// 为纯 gzip 流推断输出名称。
void TestLibArchive::gzipStreamName()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath(QStringLiteral("notes.txt"));
    QVERIFY(writeFile(source, QByteArrayLiteral("compressed payload")));

    const QString archive = temp.filePath(QStringLiteral("notes.txt.gz"));
    QVERIFY(runTool(m_7z, {QStringLiteral("a"), QStringLiteral("-bso0"),
                           QStringLiteral("-bsp0"), QStringLiteral("-tgzip"),
                           archive, source}));

    LibArchiveBackend backend(ArchiveFormat::Gzip);
    backend.openArchive(archive);

    QList<ArchiveEntry> entries;
    const Result listed = list(backend, {}, &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("notes.txt"));

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("notes.txt"))),
             QByteArrayLiteral("compressed payload"));
}

// AES zip requests a password and then extracts.
// AES zip 请求密码后完成解压。
void TestLibArchive::aesZipNeedsPassword()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("encrypted.zip"));
    QVERIFY2(makeArchive(temp.path(),
                         {QStringLiteral("-tzip"), QStringLiteral("-psecret"),
                          QStringLiteral("-mem=AES256")},
                         archive),
             "failed to create the AES zip fixture");

    LibArchiveBackend backend(ArchiveFormat::Zip);
    backend.openArchive(archive);

    // zip never encrypts the entry names, so listing works without a password.
    QList<ArchiveEntry> entries;
    const Result listed = list(backend, {}, &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    bool hasEncryptedEntry = false;
    for (const ArchiveEntry &entry : entries) {
        hasEncryptedEntry = hasEncryptedEntry || entry.isEncrypted;
    }
    QVERIFY(hasEncryptedEntry);

    // Extracting without a password must ask the UI instead of failing hard.
    ExtractOptions withoutPassword;
    withoutPassword.destDir = temp.filePath(QStringLiteral("out-nopwd"));
    const Result prompted = extract(backend, withoutPassword);
    QVERIFY(prompted.passwordRequested);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.password = QStringLiteral("secret");
    const Result extracted = extract(backend, extractOptions);
    QVERIFY2(extracted.success, qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("src/a.txt"))),
             QByteArrayLiteral("alpha"));
}

// Encrypted 7z is rejected so the manager can fall back.
// 拒绝加密 7z，以便管理器回退。
void TestLibArchive::encrypted7zIsNotSupported()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("encrypted.7z"));
    QVERIFY2(makeArchive(temp.path(),
                         {QStringLiteral("-psecret"),
                          QStringLiteral("-mhe=on")},
                         archive),
             "failed to create the encrypted 7z fixture");

    LibArchiveBackend backend(ArchiveFormat::SevenZip);
    backend.openArchive(archive);

    QList<ArchiveEntry> entries;
    const Result listed = list(backend, {}, &entries);
    QVERIFY(!listed.success);
    // UnsupportedFormat is what makes ArchiveManager switch to the CLI backend.
    QCOMPARE(listed.code, ArchiveErrorCode::UnsupportedFormat);
}

// Corrupted data is reported as an archive error.
// 损坏数据被报告为压缩包错误。
void TestLibArchive::corruptedArchiveIsDetected()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("broken.zip"));
    QVERIFY2(makeArchive(temp.path(), {QStringLiteral("-tzip")}, archive),
             "failed to create the zip fixture");

    QFile file(archive);
    QVERIFY(file.open(QIODevice::ReadWrite));
    // Flip bytes inside the entry data so the CRC check has to fail (a plain
    // truncation can still be read as a stream by libarchive).
    QVERIFY(file.seek(file.size() / 2));
    QCOMPARE(file.write(QByteArray(64, '\xff')), 64);
    file.close();

    LibArchiveBackend backend(ArchiveFormat::Zip);
    backend.openArchive(archive);
    const Result tested = test(backend, {});
    QVERIFY(!tested.success);
}

// Only the selected member is extracted.
// 仅解压选中的成员。
void TestLibArchive::selectedExtractionOnly()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("partial.zip"));
    QVERIFY2(makeArchive(temp.path(), {QStringLiteral("-tzip")}, archive),
             "failed to create the zip fixture");

    LibArchiveBackend backend(ArchiveFormat::Zip);
    backend.openArchive(archive);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    options.selectedFiles = {QStringLiteral("src/sub/b.txt")};
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));

    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("src/sub/b.txt"))),
             QByteArrayLiteral("bravo"));
    QVERIFY(!QFileInfo::exists(QDir(dest).filePath(QStringLiteral("src/a.txt"))));
}

// Selecting a directory extracts its subtree.
// 选中目录时解压其子树。
void TestLibArchive::directorySelectionExtractsSubtree()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("tree.zip"));
    QVERIFY2(makeArchive(temp.path(), {QStringLiteral("-tzip")}, archive),
             "failed to create the zip fixture");

    LibArchiveBackend backend(ArchiveFormat::Zip);
    backend.openArchive(archive);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    // Selecting a directory must extract the whole subtree, like the 7z CLI.
    options.selectedFiles = {QStringLiteral("src")};
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));

    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("src/a.txt"))),
             QByteArrayLiteral("alpha"));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("src/sub/b.txt"))),
             QByteArrayLiteral("bravo"));
}

QTEST_GUILESS_MAIN(TestLibArchive)

#include "tst_libarchive.moc"
