// QArchiver / 小Q解压
// Integration tests for tar, zip, 7z, RAR, stream and encoding backends.
// tar、zip、7z、RAR、流与编码后端的集成测试。

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "backends/sevenzip/SevenZipBackend.h"
#include "backends/rar/RarBackend.h"
#include "backends/stream/StreamBackend.h"
#include "backends/tar/TarBackend.h"
#include "backends/zip/ZipBackend.h"
#include "core/ArchiveOptions.h"
#include "utils/MimeTypeHelper.h"

namespace {

#ifdef Q_OS_LINUX
void appendLe16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>(value & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
}

void appendLe32(QByteArray &out, quint32 value)
{
    out.append(static_cast<char>(value & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
    out.append(static_cast<char>((value >> 16) & 0xff));
    out.append(static_cast<char>((value >> 24) & 0xff));
}

quint32 crc32(const QByteArray &data)
{
    quint32 crc = 0xffffffffu;
    for (const char ch : data) {
        crc ^= static_cast<unsigned char>(ch);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

// Minimal stored ZIP with the filename bytes stored verbatim (no UTF-8 flag).
// Used to exercise legacy GBK member names on Linux.
QByteArray makeStoredZip(const QByteArray &nameBytes, const QByteArray &data)
{
    const quint32 crc = crc32(data);
    const quint16 nameLength = static_cast<quint16>(nameBytes.size());
    const quint32 size = static_cast<quint32>(data.size());

    QByteArray local;
    appendLe32(local, 0x04034b50u);
    appendLe16(local, 20);
    appendLe16(local, 0);
    appendLe16(local, 0);
    appendLe16(local, 0);
    appendLe16(local, 0);
    appendLe32(local, crc);
    appendLe32(local, size);
    appendLe32(local, size);
    appendLe16(local, nameLength);
    appendLe16(local, 0);
    local.append(nameBytes);
    local.append(data);

    QByteArray central;
    appendLe32(central, 0x02014b50u);
    appendLe16(central, 20);
    appendLe16(central, 20);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe32(central, crc);
    appendLe32(central, size);
    appendLe32(central, size);
    appendLe16(central, nameLength);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe16(central, 0);
    appendLe32(central, 0);
    appendLe32(central, 0);
    central.append(nameBytes);

    QByteArray eocd;
    appendLe32(eocd, 0x06054b50u);
    appendLe16(eocd, 0);
    appendLe16(eocd, 0);
    appendLe16(eocd, 1);
    appendLe16(eocd, 1);
    appendLe32(eocd, static_cast<quint32>(central.size()));
    appendLe32(eocd, static_cast<quint32>(local.size()));
    appendLe16(eocd, 0);

    return local + central + eocd;
}
#endif // Q_OS_LINUX

} // namespace

class TestBackends : public QObject {
    Q_OBJECT

private slots:
    void tarRoundTrip();
    void tarCompressedRoundTrip_data();
    void tarCompressedRoundTrip();
    void tarAddDelete();
    void tarStreamDetection();
    void tarDotPrefixAndNonAscii();
    void tarUnsafeSymlinkCleansTemporaryTree();
    void rarBackendCapabilities();
    void rarCreateArchive();
    void rarCreateSplitVolumes();
    void rarSplitVolumes_data();
    void rarSplitVolumes();
    void rarSplitMissingVolume();
    void zipRoundTrip();
    void sevenZipRoundTrip();
    void sevenZipSpecialNames();
    void sevenZipExcludePatterns();
#ifdef Q_OS_LINUX
    void legacyGbkZipSelectedExtract();
#endif
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

    static OperationResult addFiles(ArchiveBackend &backend,
                                    const AddFilesOptions &options)
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
        backend.addFiles(options);
        QObject::disconnect(finishedConnection);
        QObject::disconnect(errorConnection);
        return result;
    }

    static OperationResult removeEntries(ArchiveBackend &backend,
                                         const DeleteFilesOptions &options)
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
        backend.deleteFiles(options);
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

    static QByteArray readBytes(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            return QByteArray();
        }
        return file.readAll();
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

// Create, list and extract a plain tar archive.
// 创建、列出并解压普通 tar 压缩包。
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

// Compression variants for the tar round-trip test.
// tar 往返测试的压缩格式变体。
void TestBackends::tarCompressedRoundTrip_data()
{
    QTest::addColumn<int>("format");
    QTest::addColumn<QString>("suffix");

    QTest::newRow("gzip") << int(ArchiveFormat::TarGz)
                          << QStringLiteral(".tar.gz");
    QTest::newRow("xz") << int(ArchiveFormat::TarXz)
                        << QStringLiteral(".tar.xz");
    QTest::newRow("bzip2") << int(ArchiveFormat::TarBz2)
                           << QStringLiteral(".tar.bz2");
}

// Two-stage tar.gz/tar.xz/tar.bz2 creation and extraction.
// 两阶段创建与解压 tar.gz/tar.xz/tar.bz2。
void TestBackends::tarCompressedRoundTrip()
{
    QFETCH(int, format);
    QFETCH(QString, suffix);

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString a = temp.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeFile(a, QByteArrayLiteral("compressed")));

    const QString archive = temp.filePath(QStringLiteral("test") + suffix);
    TarBackend backend(static_cast<ArchiveFormat>(format));
    CompressOptions options;
    options.archivePath = archive;
    options.sourceFiles = {a};
    options.compressionLevel = 6;
    const auto compressed = compress(backend, options);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));
    QVERIFY(QFile::exists(archive));

    TarBackend reader(static_cast<ArchiveFormat>(format));
    reader.openArchive(archive);
    QVERIFY(!reader.supportsAppend());

    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "compressed tar list failed");
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("a.txt"));
    QVERIFY(!entries.first().isDir);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(dest + QStringLiteral("/a.txt")),
             QStringLiteral("compressed"));
}

// Append and delete entries in an uncompressed tar.
// 在未压缩 tar 中追加与删除条目。
void TestBackends::tarAddDelete()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString a = temp.filePath(QStringLiteral("a.txt"));
    const QString b = temp.filePath(QStringLiteral("b.txt"));
    QVERIFY(writeFile(a, QByteArrayLiteral("first")));
    QVERIFY(writeFile(b, QByteArrayLiteral("second")));

    const QString archive = temp.filePath(QStringLiteral("test.tar"));
    TarBackend creator(ArchiveFormat::Tar);
    CompressOptions compressOptions;
    compressOptions.archivePath = archive;
    compressOptions.sourceFiles = {a};
    const auto compressed = compress(creator, compressOptions);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));

    TarBackend writer(ArchiveFormat::Tar);
    writer.openArchive(archive);
    QVERIFY(writer.supportsAppend());

    AddFilesOptions addOptions;
    addOptions.archivePath = archive;
    addOptions.sourceFiles = {b};
    addOptions.followSymlinks = true;
    const auto added = addFiles(writer, addOptions);
    QVERIFY2(added.finished && added.success, qPrintable(added.message));

    DeleteFilesOptions deleteOptions;
    deleteOptions.archivePath = archive;
    deleteOptions.targetEntries = {QStringLiteral("a.txt")};
    const auto deleted = removeEntries(writer, deleteOptions);
    QVERIFY2(deleted.finished && deleted.success, qPrintable(deleted.message));
    writer.closeArchive();

    TarBackend reader(ArchiveFormat::Tar);
    reader.openArchive(archive);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "tar list after add/delete failed");
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("b.txt"));

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(dest + QStringLiteral("/b.txt")),
             QStringLiteral("second"));
}

// Detect a renamed tar.gz stream and distinguish plain gzip.
// 检测重命名后的 tar.gz 流，并区分普通 gzip。
void TestBackends::tarStreamDetection()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString source = temp.filePath(QStringLiteral("payload.txt"));
    QVERIFY(writeFile(source, QByteArrayLiteral("payload")));

    const QString innerTar = temp.filePath(QStringLiteral("inner.tar"));
    TarBackend tarWriter(ArchiveFormat::Tar);
    CompressOptions tarOptions;
    tarOptions.archivePath = innerTar;
    tarOptions.sourceFiles = {source};
    const auto tarCompressed = compress(tarWriter, tarOptions);
    QVERIFY2(tarCompressed.finished && tarCompressed.success,
             qPrintable(tarCompressed.message));

    // A gzip stream whose decompressed payload is a tar, but with a plain
    // .gz suffix so detection has to probe the stream.
    const QString tarGz = temp.filePath(QStringLiteral("payload.tar.gz"));
    TarBackend gzWriter(ArchiveFormat::TarGz);
    CompressOptions gzOptions;
    gzOptions.archivePath = tarGz;
    gzOptions.sourceFiles = {innerTar};
    const auto gzCompressed = compress(gzWriter, gzOptions);
    QVERIFY2(gzCompressed.finished && gzCompressed.success,
             qPrintable(gzCompressed.message));

    const QString disguised = temp.filePath(QStringLiteral("payload.gz"));
    QVERIFY(QFile::rename(tarGz, disguised));
    QCOMPARE(MimeTypeHelper::detect(disguised), ArchiveFormat::TarGz);

    const QString plainGz = temp.filePath(QStringLiteral("plain.gz"));
    QProcess gzip;
    gzip.start(QStringLiteral("gzip"), {QStringLiteral("-c"), source});
    QVERIFY(gzip.waitForFinished(10000));
    QCOMPARE(gzip.exitCode(), 0);
    QVERIFY(writeFile(plainGz, gzip.readAllStandardOutput()));
    QCOMPARE(MimeTypeHelper::detect(plainGz), ArchiveFormat::Gzip);
}

// Read GNU tar entries with "./" prefixes and non-ASCII names.
// 读取带 "./" 前缀及非 ASCII 名称的 GNU tar 条目。
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
    const QString tarProgram = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (tarProgram.isEmpty()) {
        QSKIP("system tar is not available for the compatibility fixture");
    }

    const QString archive = temp.filePath(QStringLiteral("dot-prefix.tar.gz"));
    QProcess tar;
    tar.start(tarProgram,
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

// RAR is read-only, supports RAR4/RAR5 and rejects writes.
// RAR 为只读，支持 RAR4/RAR5，并拒绝写操作。
// The backend-level move-time check rejects an escaping symlink and removes
// the temporary extraction tree.
// 后端在移动前拒绝逃逸符号链接，并删除临时解压目录。
void TestBackends::tarUnsafeSymlinkCleansTemporaryTree()
{
    const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (tar.isEmpty()) {
        QSKIP("the tar tool is required for this test");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString src = temp.filePath(QStringLiteral("src"));
    QVERIFY(QDir().mkpath(src));
    QVERIFY(writeFile(QDir(src).filePath(QStringLiteral("safe.txt")),
                      QByteArrayLiteral("safe")));
    QVERIFY(QFile::link(QStringLiteral("../outside.txt"),
                        QDir(src).filePath(QStringLiteral("escape"))));

    const QString archive = temp.filePath(QStringLiteral("unsafe.tar"));
    {
        QProcess process;
        process.start(tar, {QStringLiteral("-cf"), archive,
                            QStringLiteral("-C"), src, QStringLiteral(".")});
        QVERIFY(process.waitForFinished(30000));
        QCOMPARE(process.exitCode(), 0);
    }

    TarBackend backend(ArchiveFormat::Tar);
    backend.openArchive(archive);
    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    options.overwrite = OverwritePolicy::Always;
    const OperationResult result = extract(backend, options);
    QVERIFY(result.finished);
    QVERIFY(!result.success);
    QVERIFY(!QFileInfo::exists(QDir(dest).filePath(QStringLiteral("escape"))));
    const QStringList temporary = QDir(dest).entryList(
        {QStringLiteral(".tmp_extract_*")}, QDir::Dirs | QDir::Hidden);
    QVERIFY2(temporary.isEmpty(), qPrintable(temporary.join(QStringLiteral(", "))));
}

void TestBackends::rarBackendCapabilities()
{
    RarBackend rar4(ArchiveFormat::Rar4);
    QCOMPARE(rar4.format(), ArchiveFormat::Rar4);
    QVERIFY(rar4.isReadOnly());
    QVERIFY(!rar4.supportsAppend());
    QVERIFY(rar4.supportsEncryption());
    QVERIFY(rar4.supportsVolume());

    RarBackend rar5(ArchiveFormat::Rar5);
    QCOMPARE(rar5.format(), ArchiveFormat::Rar5);

    if (!RarBackend::isCreateAvailable()) {
        CompressOptions options;
        options.archivePath = QStringLiteral("/tmp/qarchiver-unused.rar");
        options.sourceFiles = {QStringLiteral("/tmp/qarchiver-unused.txt")};
        const auto result = compress(rar5, options);
        QVERIFY(result.finished);
        QVERIFY(!result.success);
    }
}

// Creating RAR archives needs the system rar command; skip when absent.
// 创建 RAR 压缩包需要系统 rar 命令；未安装时跳过。
void TestBackends::rarCreateArchive()
{
    if (!RarBackend::isCreateAvailable()) {
        QSKIP("the rar command is not installed");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath(QStringLiteral("created 中文.txt"));
    const QByteArray payload = QByteArrayLiteral("created by the rar command");
    QVERIFY(writeFile(source, payload));

    const QString archive = temp.filePath(QStringLiteral("created.rar"));
    RarBackend writer(ArchiveFormat::Rar5);
    CompressOptions options;
    options.archivePath = archive;
    options.sourceFiles = {source};
    options.compressionLevel = 6;
    const auto compressed = compress(writer, options);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));
    QVERIFY(QFileInfo::exists(archive));
    QCOMPARE(MimeTypeHelper::detect(archive), ArchiveFormat::Rar5);
    QVERIFY(!MimeTypeHelper::isSplitVolume(archive));

    RarBackend reader(ArchiveFormat::Rar5);
    reader.openArchive(archive);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "created RAR list failed");
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("created 中文.txt"));

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readBytes(QDir(dest).filePath(QStringLiteral("created 中文.txt"))),
             payload);
}

// RAR volumes must use the part1.rar / part2.rar naming and stay readable.
// RAR 分卷必须使用 part1.rar / part2.rar 命名，并且可正常读取。
void TestBackends::rarCreateSplitVolumes()
{
    if (!RarBackend::isCreateAvailable()) {
        QSKIP("the rar command is not installed");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath(QStringLiteral("volume-payload.bin"));
    QByteArray payload;
    payload.reserve(10000);
    for (int i = 0; i < 10000; ++i) {
        payload.append(static_cast<char>((i * 29 + 7) & 0xff));
    }
    QVERIFY(writeFile(source, payload));

    const QString archive = temp.filePath(QStringLiteral("split.rar"));
    RarBackend writer(ArchiveFormat::Rar5);
    CompressOptions options;
    options.archivePath = archive;
    options.sourceFiles = {source};
    options.compressionLevel = 0;
    options.volumeSize = 3000;
    const auto compressed = compress(writer, options);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));

    const QString first = temp.filePath(QStringLiteral("split.part1.rar"));
    const QString second = temp.filePath(QStringLiteral("split.part2.rar"));
    QVERIFY(QFileInfo::exists(first));
    QVERIFY(QFileInfo::exists(second));
    QVERIFY(!QFileInfo::exists(archive));
    QVERIFY(MimeTypeHelper::isSplitVolume(second));
    QCOMPARE(QFileInfo(MimeTypeHelper::resolveFirstVolume(second)).canonicalFilePath(),
             QFileInfo(first).canonicalFilePath());

    RarBackend reader(ArchiveFormat::Rar5);
    reader.openArchive(second);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "created split RAR list failed");
    QCOMPARE(entries.size(), 1);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readBytes(QDir(dest).filePath(QStringLiteral("volume-payload.bin"))),
             payload);
}

void TestBackends::rarSplitVolumes_data()
{
    QTest::addColumn<QString>("volume");
    QTest::addColumn<QString>("firstVolume");
    QTest::addColumn<ArchiveFormat>("format");

    QTest::newRow("rar5-part") << QStringLiteral("rar5_split/data.part2.rar")
                                << QStringLiteral("rar5_split/data.part1.rar")
                                << ArchiveFormat::Rar5;
    QTest::newRow("rar4-r00") << QStringLiteral("rar4_split/data.r01")
                               << QStringLiteral("rar4_split/data.rar")
                               << ArchiveFormat::Rar4;
}

// Split RAR volumes: opening a non-first volume resolves the first one and
// then lists/extracts through the bundled 7-Zip helper.
// RAR 分卷：从非首卷打开时解析首卷，再经内置 7-Zip 列出/解压。
void TestBackends::rarSplitVolumes()
{
    QFETCH(QString, volume);
    QFETCH(QString, firstVolume);
    QFETCH(ArchiveFormat, format);

#ifndef QARCHIVER_TEST_DATA_DIR
#define QARCHIVER_TEST_DATA_DIR "test_files"
#endif
    const QDir dataDir(QStringLiteral(QARCHIVER_TEST_DATA_DIR));
    const QString volumePath = dataDir.filePath(volume);
    const QString firstPath = dataDir.filePath(firstVolume);
    QVERIFY2(QFileInfo::exists(volumePath), qPrintable(volumePath));
    QVERIFY2(QFileInfo::exists(firstPath), qPrintable(firstPath));

    QCOMPARE(MimeTypeHelper::detect(volumePath), format);
    QVERIFY(MimeTypeHelper::isSplitVolume(volumePath));
    QCOMPARE(QFileInfo(MimeTypeHelper::resolveFirstVolume(volumePath)).canonicalFilePath(),
             QFileInfo(firstPath).canonicalFilePath());

    RarBackend reader(format);
    reader.openArchive(volumePath);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "split RAR list failed");
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("payload.bin"));

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    ExtractOptions options;
    options.destDir = temp.filePath(QStringLiteral("out"));
    options.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, options);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));

    QByteArray expected;
    expected.reserve(8000);
    for (int i = 0; i < 8000; ++i) {
        expected.append(static_cast<char>((i * 37 + 11) & 0xff));
    }
    QCOMPARE(readBytes(QDir(options.destDir).filePath(QStringLiteral("payload.bin"))),
             expected);
}

// A missing middle volume must surface as VolumeMissing, not Unknown.
// 缺失中间分卷时必须报告 VolumeMissing，而不是 Unknown。
void TestBackends::rarSplitMissingVolume()
{
#ifndef QARCHIVER_TEST_DATA_DIR
#define QARCHIVER_TEST_DATA_DIR "test_files"
#endif
    const QDir dataDir(QStringLiteral(QARCHIVER_TEST_DATA_DIR) +
                       QStringLiteral("/rar5_split"));
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    for (const QString &name : {QStringLiteral("data.part1.rar"),
                                QStringLiteral("data.part3.rar")}) {
        QVERIFY(QFile::copy(dataDir.filePath(name), temp.filePath(name)));
    }

    RarBackend reader(ArchiveFormat::Rar5);
    reader.openArchive(temp.filePath(QStringLiteral("data.part1.rar")));

    bool finished = false;
    ArchiveErrorCode code = ArchiveErrorCode::Unknown;
    connect(&reader, &ArchiveBackend::fileListReady, &reader,
            [&finished](const QList<ArchiveEntry> &) { finished = true; });
    connect(&reader, &ArchiveBackend::errorOccurred, &reader,
            [&finished, &code](ArchiveErrorCode errorCode, const QString &) {
                finished = true;
                code = errorCode;
            });
    reader.listContents();

    QVERIFY(finished);
    QCOMPARE(code, ArchiveErrorCode::VolumeMissing);
}

// Create, list, extract and test a zip archive.
// 创建、列出、解压并测试 zip 压缩包。
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

// Create, list, extract and test a 7z archive.
// 创建、列出、解压并测试 7z 压缩包。
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

// Handle wildcard, dash, @ and non-ASCII member names safely.
// 安全处理通配符、短横线、@ 及非 ASCII 成员名。
void TestBackends::sevenZipSpecialNames()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString sourceDir = temp.filePath(QStringLiteral("special"));
    QVERIFY(QDir().mkpath(sourceDir));

    const QList<QPair<QString, QByteArray>> files = {
        {QStringLiteral("-dash.txt"), QByteArrayLiteral("dash")},
        {QStringLiteral("star*name.txt"), QByteArrayLiteral("star")},
        {QStringLiteral("quest?name.txt"), QByteArrayLiteral("quest")},
        {QStringLiteral("at@name.txt"), QByteArrayLiteral("at")},
        {QStringLiteral("中文 空格.txt"), QByteArrayLiteral("chinese")},
    };
    for (const auto &file : files) {
        QVERIFY2(writeFile(QDir(sourceDir).filePath(file.first), file.second),
                 qPrintable(file.first));
    }

    const QString archive = temp.filePath(QStringLiteral("special.7z"));
    SevenZipBackend writer;
    CompressOptions compressOptions;
    compressOptions.archivePath = archive;
    for (const auto &file : files) {
        compressOptions.sourceFiles << QDir(sourceDir).filePath(file.first);
    }
    const auto compressed = compress(writer, compressOptions);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));

    SevenZipBackend reader;
    reader.openArchive(archive);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "special-name 7z list failed");
    QCOMPARE(entries.size(), files.size());

    QStringList names;
    for (const ArchiveEntry &entry : entries) {
        names << entry.path;
    }
    for (const auto &file : files) {
        QVERIFY2(names.contains(file.first),
                 qPrintable(QStringLiteral("missing entry: %1").arg(file.first)));
    }

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions extractOptions;
    extractOptions.destDir = dest;
    extractOptions.selectedFiles = {
        QStringLiteral("star*name.txt"),
        QStringLiteral("-dash.txt"),
        QStringLiteral("at@name.txt"),
        QStringLiteral("中文 空格.txt"),
    };
    extractOptions.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, extractOptions);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("star*name.txt"))),
             QStringLiteral("star"));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("-dash.txt"))),
             QStringLiteral("dash"));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("at@name.txt"))),
             QStringLiteral("at"));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("中文 空格.txt"))),
             QStringLiteral("chinese"));

    // Exercise the add-files path with a literal '*' in the source name.
    const QString extra = QDir(sourceDir).filePath(QStringLiteral("added*star.txt"));
    QVERIFY(writeFile(extra, QByteArrayLiteral("added")));
    SevenZipBackend adder;
    adder.openArchive(archive);
    AddFilesOptions addOptions;
    addOptions.archivePath = archive;
    addOptions.sourceFiles = {extra};
    const auto added = addFiles(adder, addOptions);
    QVERIFY2(added.finished && added.success, qPrintable(added.message));

    DeleteFilesOptions deleteOptions;
    deleteOptions.archivePath = archive;
    deleteOptions.targetEntries = {
        QStringLiteral("-dash.txt"),
        QStringLiteral("star*name.txt"),
    };
    const auto removed = removeEntries(reader, deleteOptions);
    QVERIFY2(removed.finished && removed.success,
             qPrintable(removed.message));

    SevenZipBackend afterDelete;
    afterDelete.openArchive(archive);
    bool listedAfterDelete = false;
    const auto remaining = list(afterDelete, &listedAfterDelete);
    QVERIFY2(listedAfterDelete, "special-name 7z list after delete failed");
    QCOMPARE(remaining.size(), files.size() - 1);
    bool foundAdded = false;
    for (const ArchiveEntry &entry : remaining) {
        QVERIFY2(entry.path != QStringLiteral("-dash.txt")
                     && entry.path != QStringLiteral("star*name.txt"),
                 qPrintable(QStringLiteral("entry was not deleted: %1").arg(entry.path)));
        foundAdded = foundAdded || entry.path == QStringLiteral("added*star.txt");
    }
    QVERIFY(foundAdded);
}

// Exclude patterns must still work without -spd.
// 排除规则在未使用 -spd 时仍须生效。
void TestBackends::sevenZipExcludePatterns()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QString keep = temp.filePath(QStringLiteral("keep.txt"));
    const QString skip = temp.filePath(QStringLiteral("skip.tmp"));
    QVERIFY(writeFile(keep, QByteArrayLiteral("keep")));
    QVERIFY(writeFile(skip, QByteArrayLiteral("skip")));

    const QString archive = temp.filePath(QStringLiteral("exclude.7z"));
    SevenZipBackend writer;
    CompressOptions options;
    options.archivePath = archive;
    options.sourceFiles = {keep, skip};
    options.excludePatterns = {QStringLiteral("*.tmp")};
    const auto compressed = compress(writer, options);
    QVERIFY2(compressed.finished && compressed.success,
             qPrintable(compressed.message));

    SevenZipBackend reader;
    reader.openArchive(archive);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "exclude-pattern 7z list failed");
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("keep.txt"));
}

#ifdef Q_OS_LINUX
// Decode GBK names and extract selected members via a listfile.
// 解码 GBK 名称并通过列表文件解压选定成员。
void TestBackends::legacyGbkZipSelectedExtract()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    // "中文测试.txt" encoded as GBK, without the ZIP UTF-8 flag.
    const QByteArray gbkName =
        QByteArray::fromHex("d6d0cec4b2e2cad42e747874");
    const QString archive = temp.filePath(QStringLiteral("legacy-gbk.zip"));
    QVERIFY(writeFile(archive, makeStoredZip(gbkName, QByteArrayLiteral("legacy"))));

    SevenZipBackend reader(ArchiveFormat::Zip);
    reader.openArchive(archive);
    bool listed = false;
    const auto entries = list(reader, &listed);
    QVERIFY2(listed, "legacy GBK zip list failed");
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, QStringLiteral("中文测试.txt"));

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    options.selectedFiles = {entries.first().path};
    options.overwrite = OverwritePolicy::Always;
    const auto extracted = extract(reader, options);
    QVERIFY2(extracted.finished && extracted.success,
             qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("中文测试.txt"))),
             QStringLiteral("legacy"));
}
#endif

// Read a pure gzip stream as a single entry.
// 将纯 gzip 流作为单个条目读取。
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
