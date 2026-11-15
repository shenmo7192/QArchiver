#include <QtTest>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

#include "backends/sevenzip/SevenZipBackend.h"
#include "backends/stream/StreamBackend.h"
#include "backends/tar/TarBackend.h"
#include "backends/zip/ZipBackend.h"
#include "core/ArchiveOptions.h"

class TestBackends : public QObject {
    Q_OBJECT

private slots:
    void tarRoundTrip();
    void tarGzRoundTrip();
    void tarDotPrefixAndNonAscii();
    void zipRoundTrip();
    void sevenZipRoundTrip();
    void gzipStream();

private:
    struct OperationResult {
        bool finished = false;
        bool success = false;
        QString message;
    };

    static OperationResult compress(ArchiveBackend &backend,
                                    const CompressOptions &options)
    {
        OperationResult result;
        const auto finishedConnection = QObject::connect(
            &backend, &ArchiveBackend::operationFinished,
            &backend, [&result](bool ok, const QString &msg) {
                result.finished = true;
                result.success = ok;
                result.message = msg;
            });
        const auto errorConnection = QObject::connect(
            &backend, &ArchiveBackend::errorOccurred,
            &backend, [&result](ArchiveErrorCode, const QString &msg) {
                result.finished = true;
                result.success = false;
                result.message = msg;
            });
        backend.compress(options);
        QObject::disconnect(finishedConnection);
        QObject::disconnect(errorConnection);
        return result;
    }

    static OperationResult extract(ArchiveBackend &backend,
                                   const ExtractOptions &options)
    {
        OperationResult result;
        const auto finishedConnection = QObject::connect(
            &backend, &ArchiveBackend::operationFinished,
            &backend, [&result](bool ok, const QString &msg) {
                result.finished = true;
                result.success = ok;
                result.message = msg;
            });
        const auto errorConnection = QObject::connect(
            &backend, &ArchiveBackend::errorOccurred,
            &backend, [&result](ArchiveErrorCode, const QString &msg) {
                result.finished = true;
                result.success = false;
                result.message = msg;
            });
        backend.extract(options);
        QObject::disconnect(finishedConnection);
        QObject::disconnect(errorConnection);
        return result;
    }

    static QList<ArchiveEntry> list(ArchiveBackend &backend, bool *ok)
    {
        QList<ArchiveEntry> entries;
        bool finished = false;
        *ok = false;
        const auto listConnection = QObject::connect(
            &backend, &ArchiveBackend::fileListReady,
            &backend, [&entries, &finished](const QList<ArchiveEntry> &list) {
                entries = list;
                finished = true;
            });
        const auto errorConnection = QObject::connect(
            &backend, &ArchiveBackend::errorOccurred,
            &backend, [&finished](ArchiveErrorCode, const QString &) {
                finished = true;
            });
        backend.listContents();
        QObject::disconnect(listConnection);
        QObject::disconnect(errorConnection);
        *ok = finished && !entries.isEmpty();
        return entries;
    }

    static QString readFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QString();
        }
        return QString::fromUtf8(file.readAll());
    }

    static bool writeFile(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(data) == data.size();
    }
};

void TestBackends::tarRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString a = temp.filePath(QStringLiteral("a.txt"));
    const QString b = temp.filePath(QStringLiteral("b.txt"));
    QVERIFY(writeFile(a, QByteArrayLiteral("hello")));
    QVERIFY(writeFile(b, QByteArrayLiteral("world")));

    const QString archive = temp.filePath(QStringLiteral("test.tar"));
    TarBackend backend(ArchiveFormat::Tar);

    CompressOptions compressOptions;
    compressOptions.archivePath = archive;
    compressOptions.sourceFiles = {a, b};
    const auto compressed = compress(backend, compressOptions);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));

    TarBackend reader(ArchiveFormat::Tar);
    reader.openArchive(archive);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "tar list failed");
    QCOMPARE(entries.size(), 2);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(dest + QStringLiteral("/a.txt")), QStringLiteral("hello"));
    QCOMPARE(readFile(dest + QStringLiteral("/b.txt")), QStringLiteral("world"));
}

void TestBackends::tarGzRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString a = temp.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeFile(a, QByteArrayLiteral("gzip")));

    const QString archive = temp.filePath(QStringLiteral("test.tar.gz"));
    TarBackend backend(ArchiveFormat::TarGz);
    CompressOptions options;
    options.archivePath = archive;
    options.sourceFiles = {a};
    const auto compressed = compress(backend, options);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));

    TarBackend reader(ArchiveFormat::TarGz);
    reader.openArchive(archive);
    bool listed = false;
    list(reader, &listed);
    QVERIFY(listed);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(dest + QStringLiteral("/a.txt")), QStringLiteral("gzip"));
}

void TestBackends::tarDotPrefixAndNonAscii()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceDir = temp.filePath(QStringLiteral("source"));
    QVERIFY(QDir().mkpath(sourceDir));
    const QString fileName = QStringLiteral("动画片_15.json");
    const QByteArray payload = QByteArrayLiteral("unicode-name");
    QVERIFY(writeFile(QDir(sourceDir).filePath(fileName), payload));

    // Create with "." so members are stored as "./动画片_15.json".  Combined
    // with LC_ALL=C this used to make tar octal-escape the UTF-8 name.
    const QString archive = temp.filePath(QStringLiteral("dot-prefix.tar.gz"));
    QProcess tar;
    tar.start(QStringLiteral("tar"),
              {QStringLiteral("-czf"), archive, QStringLiteral("-C"),
               sourceDir, QStringLiteral(".")});
    QVERIFY(tar.waitForFinished(10000));
    QCOMPARE(tar.exitCode(), 0);

    TarBackend reader(ArchiveFormat::TarGz);
    reader.openArchive(archive);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "tar list failed");

    ArchiveEntry target;
    for (const ArchiveEntry &entry : entries) {
        if (entry.path.contains(fileName)) {
            target = entry;
            break;
        }
    }
    QVERIFY2(!target.path.isEmpty(), "non-ASCII entry not found");
    QVERIFY2(!target.path.contains(QLatin1Char('\\')),
             qPrintable(QStringLiteral("entry was escaped: %1").arg(target.path)));

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    options.selectedFiles = QStringList{target.path};
    options.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, options);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(fileName)), QString::fromUtf8(payload));
}

void TestBackends::zipRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString a = temp.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeFile(a, QByteArrayLiteral("zip")));

    const QString archive = temp.filePath(QStringLiteral("test.zip"));
    ZipBackend backend;
    CompressOptions options;
    options.archivePath = archive;
    options.sourceFiles = {a};
    options.compressionLevel = 5;
    const auto compressed = compress(backend, options);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));

    ZipBackend reader;
    reader.openArchive(archive);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "zip list failed");
    QCOMPARE(entries.size(), 1);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(dest + QStringLiteral("/a.txt")), QStringLiteral("zip"));
}

void TestBackends::sevenZipRoundTrip()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString a = temp.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeFile(a, QByteArrayLiteral("sevenzip")));

    const QString archive = temp.filePath(QStringLiteral("test.7z"));
    SevenZipBackend backend;
    CompressOptions options;
    options.archivePath = archive;
    options.sourceFiles = {a};
    options.compressionLevel = 7;
    const auto compressed = compress(backend, options);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));

    SevenZipBackend reader;
    reader.openArchive(archive);
    bool listed = false;
    list(reader, &listed);
    QVERIFY(listed);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(dest + QStringLiteral("/a.txt")), QStringLiteral("sevenzip"));
}

void TestBackends::gzipStream()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath(QStringLiteral("stream.txt"));
    QVERIFY(writeFile(source, QByteArrayLiteral("stream-data")));

    const QString gz = temp.filePath(QStringLiteral("stream.txt.gz"));
    QProcess gzip;
    gzip.start(QStringLiteral("gzip"), {QStringLiteral("-c"), source});
    QVERIFY(gzip.waitForFinished(10000));
    QCOMPARE(gzip.exitCode(), 0);
    QVERIFY(writeFile(gz, gzip.readAllStandardOutput()));

    StreamBackend backend(ArchiveFormat::Gzip);
    backend.openArchive(gz);
    bool listed = false;
    const auto entries = list(backend, &listed);
    QVERIFY2(listed, "gzip stream list failed");
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("stream.txt"));

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    options.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(backend, options);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(dest + QStringLiteral("/stream.txt")),
             QStringLiteral("stream-data"));
}

QTEST_GUILESS_MAIN(TestBackends)

#include "tst_backends.moc"
