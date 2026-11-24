// QArchiver / 小Q解压
// Tests for the optional in-process libarchive backend.
// 可选进程内 libarchive 后端的测试。

#include <QtTest>

#include <array>

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

#ifndef Q_OS_WIN
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "backends/libarchive/LibArchiveBackend.h"
#include "common/ArchiveOptions.h"
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
    void hardLinksAreRecreatedSafely();
    void legacyNameEncodingIsDecoded();
    void legacyBig5NamesAreDecoded();
    void explicitEncodingHintWins();

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

namespace {

// Little-endian writers for the hand-built ZIP fixture below.
// 下方手工构造 ZIP 夹具用的小端写入函数。
void appendUInt16(QByteArray &out, quint16 value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

void appendUInt32(QByteArray &out, quint32 value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        out.append(static_cast<char>((value >> shift) & 0xFF));
    }
}

// Standard IEEE 802.3 CRC-32, needed by the ZIP headers.
// ZIP 头部需要的标准 IEEE 802.3 CRC-32。
quint32 crc32Of(const QByteArray &data)
{
    static const auto table = [] {
        std::array<quint32, 256> values {};
        for (quint32 i = 0; i < values.size(); ++i) {
            quint32 value = i;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1u) ? (0xEDB88320u ^ (value >> 1)) : (value >> 1);
            }
            values[i] = value;
        }
        return values;
    }();

    quint32 crc = 0xFFFFFFFFu;
    for (const char byte : data) {
        const auto index = (crc ^ static_cast<quint8>(byte)) & 0xFFu;
        crc = table[index] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

struct ZipMember {
    QByteArray rawName;   // name bytes exactly as stored / 按原样存储的名称字节
    QByteArray content;
};

// Minimal "stored" ZIP writer.
//
// The tests need member names that a third-party tool cannot easily produce:
// legacy bytes *without* the UTF-8 flag.  7-Zip always flags names it writes
// (it converts them to UTF-8 first), so the fixture is assembled here instead
// of shipping a pre-built archive in the repository.
// 最小化的 "stored" ZIP 写入器。
// 测试需要第三方工具不容易产生的成员名：不带 UTF-8 标志的旧式字节。
// 7-Zip 写出的名称总是带该标志（它先转成 UTF-8），因此这里自行拼装夹具，
// 而不在仓库中存放预生成的压缩包。
QByteArray buildStoredZip(const QList<ZipMember> &members)
{
    QByteArray local;
    QByteArray central;

    for (const ZipMember &member : members) {
        const quint32 crc = crc32Of(member.content);
        const auto size = static_cast<quint32>(member.content.size());
        const auto nameLength = static_cast<quint16>(member.rawName.size());
        const auto offset = static_cast<quint32>(local.size());

        appendUInt32(local, 0x04034b50u);   // local file header signature
        appendUInt16(local, 20);            // version needed to extract
        appendUInt16(local, 0);             // flags: no UTF-8 flag (bit 11)
        appendUInt16(local, 0);             // method: stored
        appendUInt16(local, 0);             // last mod time
        appendUInt16(local, 0x21);          // last mod date (1980-01-01)
        appendUInt32(local, crc);
        appendUInt32(local, size);          // compressed size
        appendUInt32(local, size);          // uncompressed size
        appendUInt16(local, nameLength);
        appendUInt16(local, 0);             // extra field length
        local.append(member.rawName);
        local.append(member.content);

        appendUInt32(central, 0x02014b50u); // central directory header
        appendUInt16(central, 20);          // version made by
        appendUInt16(central, 20);          // version needed to extract
        appendUInt16(central, 0);           // flags
        appendUInt16(central, 0);           // method: stored
        appendUInt16(central, 0);           // last mod time
        appendUInt16(central, 0x21);        // last mod date
        appendUInt32(central, crc);
        appendUInt32(central, size);
        appendUInt32(central, size);
        appendUInt16(central, nameLength);
        appendUInt16(central, 0);           // extra field length
        appendUInt16(central, 0);           // comment length
        appendUInt16(central, 0);           // disk number start
        appendUInt16(central, 0);           // internal attributes
        appendUInt32(central, 0);           // external attributes
        appendUInt32(central, offset);      // local header offset
        central.append(member.rawName);
    }

    QByteArray zip = local;
    const auto centralOffset = static_cast<quint32>(zip.size());
    zip.append(central);

    appendUInt32(zip, 0x06054b50u);         // end of central directory
    appendUInt16(zip, 0);                   // this disk
    appendUInt16(zip, 0);                   // disk with central directory
    appendUInt16(zip, static_cast<quint16>(members.size()));
    appendUInt16(zip, static_cast<quint16>(members.size()));
    appendUInt32(zip, static_cast<quint32>(central.size()));
    appendUInt32(zip, centralOffset);
    appendUInt16(zip, 0);                   // comment length
    return zip;
}

} // namespace

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

// Hard links are recreated as real hard links, never as empty files.
// 硬链接会重建为真正的硬链接，绝不再是空文件。
void TestLibArchive::hardLinksAreRecreatedSafely()
{
#ifndef Q_OS_WIN
    const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (tar.isEmpty()) {
        QSKIP("the tar tool is required for this test");
    }

    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString src = temp.filePath(QStringLiteral("src"));
    QVERIFY(QDir().mkpath(src));
    const QString original = QDir(src).filePath(QStringLiteral("original.txt"));
    const QString linked = QDir(src).filePath(QStringLiteral("linked.txt"));
    QVERIFY(writeFile(original, QByteArrayLiteral("hard-link-data")));
    QCOMPARE(::link(QFile::encodeName(original).constData(),
                    QFile::encodeName(linked).constData()), 0);

    const QString archive = temp.filePath(QStringLiteral("hardlinks.tar"));
    QVERIFY(runTool(tar, {QStringLiteral("-cf"), archive,
                          QStringLiteral("-C"), src, QStringLiteral(".")}));

    LibArchiveBackend backend(ArchiveFormat::Tar);
    backend.openArchive(archive);
    QList<ArchiveEntry> entries;
    const Result listed = list(backend, {}, &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    bool sawHardLink = false;
    for (const ArchiveEntry &entry : entries) {
        if (entry.isHardLink) {
            sawHardLink = true;
            QVERIFY(!entry.hardLinkTarget.isEmpty());
        }
    }
    QVERIFY(sawHardLink);

    const QString dest = temp.filePath(QStringLiteral("out"));
    ExtractOptions options;
    options.destDir = dest;
    options.overwrite = OverwritePolicy::Always;
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));

    const QString outOriginal = QDir(dest).filePath(QStringLiteral("original.txt"));
    const QString outLinked = QDir(dest).filePath(QStringLiteral("linked.txt"));
    QCOMPARE(readFile(outOriginal), QByteArrayLiteral("hard-link-data"));
    QCOMPARE(readFile(outLinked), QByteArrayLiteral("hard-link-data"));

    struct stat a {};
    struct stat b {};
    QCOMPARE(::stat(QFile::encodeName(outOriginal).constData(), &a), 0);
    QCOMPARE(::stat(QFile::encodeName(outLinked).constData(), &b), 0);
    QCOMPARE(a.st_ino, b.st_ino);
#else
    QSKIP("hard link assertions are Unix-only");
#endif
}

void TestLibArchive::legacyNameEncodingIsDecoded()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    // GBK/GB18030 name bytes without the UTF-8 flag.  The names contain "㐀"
    // (U+3400), whose GB18030 form is the four-byte sequence 81 39 ee 39:
    // such bytes are invalid UTF-8, Big5 and Shift-JIS, so the detected result
    // does not depend on the locale of the machine running the test.
    // 不带 UTF-8 标志的 GBK/GB18030 名称字节。“㐀”（U+3400）的 GB18030 形式是
    // 四字节序列 81 39 ee 39，该序列在 UTF-8、Big5、Shift-JIS 下均非法，
    // 因此检测结果不依赖运行测试的机器 locale。
    const QByteArray gbkName =
        QByteArray::fromHex("b2e2cad4cec4b1be8139ee395f47424b2e747874");   // 测试文本㐀_GBK.txt
    const QByteArray gb18030Name =
        QByteArray::fromHex("b2e2cad4cec4b1be325f474231383033302e747874"); // 测试文本2_GB18030.txt

    QList<ZipMember> members;
    members.append({gbkName, QByteArrayLiteral("alpha")});
    members.append({gb18030Name, QByteArrayLiteral("bravo")});

    const QString archivePath = temp.filePath(QStringLiteral("legacy-names.zip"));
    QVERIFY(writeFile(archivePath, buildStoredZip(members)));

    LibArchiveBackend backend(ArchiveFormat::Zip);
    backend.openArchive(archivePath);

    QList<ArchiveEntry> entries;
    const Result listed = list(backend, ListOptions(), &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    QCOMPARE(entries.size(), 2);
    QCOMPARE(backend.nameEncoding(), QStringLiteral("GB18030"));
    QCOMPARE(entries.at(0).path, QStringLiteral("测试文本㐀_GBK.txt"));
    QCOMPARE(entries.at(1).path, QStringLiteral("测试文本2_GB18030.txt"));
    for (const ArchiveEntry &entry : entries) {
        // A U+FFFD here means the raw bytes were decoded with the process
        // locale, which is exactly what this test guards against.
        // 出现 U+FFFD 说明原始字节被按进程 locale 解码了，正是本测试要防的情况。
        QVERIFY2(!entry.path.contains(QChar(0xFFFD)), qPrintable(entry.path));
    }

    // Extraction has to write the very same names to disk.
    // 解压落盘的文件名必须与之完全一致。
    const QString dest = temp.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(dest));
    ExtractOptions options;
    options.destDir = dest;
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("测试文本㐀_GBK.txt"))),
             QByteArrayLiteral("alpha"));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("测试文本2_GB18030.txt"))),
             QByteArrayLiteral("bravo"));
}

void TestLibArchive::legacyBig5NamesAreDecoded()
{
    // Big5 names must be recognised automatically, without any hint and
    // regardless of the machine locale (Big5 and GB18030 byte ranges overlap
    // heavily, so this relies on the plausibility ranking).
    // Big5 名称必须在无需提示、且与机器 locale 无关的情况下被正确识别
    //（Big5 与 GB18030 的字节范围高度重叠，这里依赖可信度排序）。
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    const QByteArray big5Name = QByteArray::fromHex("b4fab8d5a4e5a5f32e747874"); // 測試文件.txt
    QList<ZipMember> members;
    members.append({big5Name, QByteArrayLiteral("traditional")});

    const QString archivePath = temp.filePath(QStringLiteral("legacy-big5-auto.zip"));
    QVERIFY(writeFile(archivePath, buildStoredZip(members)));

    LibArchiveBackend backend(ArchiveFormat::Zip);
    backend.openArchive(archivePath);

    QList<ArchiveEntry> entries;
    const Result listed = list(backend, ListOptions(), &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(backend.nameEncoding(), QStringLiteral("Big5"));
    QCOMPARE(entries.at(0).path, QStringLiteral("測試文件.txt"));
    QVERIFY(!entries.at(0).path.contains(QChar(0xFFFD)));

    const QString dest = temp.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(dest));
    ExtractOptions options;
    options.destDir = dest;
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("測試文件.txt"))),
             QByteArrayLiteral("traditional"));
}

void TestLibArchive::explicitEncodingHintWins()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    // Big5 "中文.txt" (a4 a4 a4 e5) without the UTF-8 flag: Big5 and GB18030
    // overlap, so only an explicit hint decodes it reliably.
    // 不带 UTF-8 标志的 Big5 “中文.txt”（a4 a4 a4 e5）：
    // Big5 与 GB18030 有重叠，只有显式提示才能可靠解码。
    const QByteArray big5Name = QByteArray::fromHex("a4a4a4e52e747874");
    QList<ZipMember> members;
    members.append({big5Name, QByteArrayLiteral("big5")});

    const QString archivePath = temp.filePath(QStringLiteral("legacy-big5.zip"));
    QVERIFY(writeFile(archivePath, buildStoredZip(members)));

    LibArchiveBackend backend(ArchiveFormat::Zip);
    backend.openArchive(archivePath);

    ListOptions listOptions;
    listOptions.encodingHint = QStringLiteral("Big5");
    QList<ArchiveEntry> entries;
    const Result listed = list(backend, listOptions, &entries);
    QVERIFY2(listed.success, qPrintable(listed.message));
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.at(0).path, QStringLiteral("中文.txt"));
    QCOMPARE(backend.nameEncoding(), QStringLiteral("Big5"));

    // The hint must also reach extraction (the UI hands the detected encoding back).
    // 提示也必须作用于解压（界面会把检测到的编码回传）。
    const QString dest = temp.filePath(QStringLiteral("out"));
    QVERIFY(QDir().mkpath(dest));
    ExtractOptions options;
    options.destDir = dest;
    options.encodingHint = QStringLiteral("Big5");
    const Result extracted = extract(backend, options);
    QVERIFY2(extracted.success, qPrintable(extracted.message));
    QCOMPARE(readFile(QDir(dest).filePath(QStringLiteral("中文.txt"))),
             QByteArrayLiteral("big5"));
}

QTEST_GUILESS_MAIN(TestLibArchive)

#include "tst_libarchive.moc"
