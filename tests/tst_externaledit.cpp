// QArchiver / 小Q解压
// Tests for external-edit detection and the write-back of edited members.
// 外部编辑检测与编辑成员写回的测试。

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QTemporaryDir>

#include "backends/sevenzip/ZipBackend.h"
#include "common/ArchiveOptions.h"
#include "core/AppSettings.h"
#include "ui/services/ExternalEditManager.h"
#include "utils/EncodingDetector.h"
#include "utils/FileUtils.h"

#include "TestArchiveBuilder.h"

#ifndef Q_OS_WIN
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {

bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return file.readAll();
}

} // namespace

class TestExternalEdit : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void writeBackReplacesMember();
    void discardLeavesArchiveUntouched();
    void identicalRewriteIsIgnored();
    void unsafeStagePathIsRejected();
    void legacyEncodingIsStagedWithRawName();
    void legacyEncodingWriteBackReplacesMember();

private:
    QString m_settingsFile;
};

void TestExternalEdit::initTestCase()
{
    QCoreApplication::setOrganizationName(QStringLiteral("QArchiverTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_externaledit"));
    m_settingsFile = QSettings(QSettings::IniFormat, QSettings::UserScope,
                               QStringLiteral("QArchiverTests"),
                               QStringLiteral("tst_externaledit"))
                         .fileName();
    QFile::remove(m_settingsFile);
}

void TestExternalEdit::cleanupTestCase()
{
    QFile::remove(m_settingsFile);
    QDir().rmdir(QFileInfo(m_settingsFile).absolutePath());
}

void TestExternalEdit::init()
{
    AppSettings::instance().setExternalEditWatch(true);
    ExternalEditManager::instance().setSettleDelayMs(0);
}

void TestExternalEdit::cleanup()
{
    ExternalEditManager::instance().setPromptHandler(nullptr);
    ExternalEditManager::instance().setSaveHandler(nullptr);
}

// An edited member is stored back under its own archive path, so the archive
// keeps exactly one entry for it.
// 被编辑的成员按其原有压缩包路径存回，压缩包中仍只有该条目的一个副本。
void TestExternalEdit::writeBackReplacesMember()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    // Build a zip with a nested member. / 创建一个含嵌套成员的 zip。
    const QString sourceRoot = temp.filePath(QStringLiteral("src"));
    QVERIFY(writeFile(sourceRoot + QStringLiteral("/sub/a.txt"),
                      QByteArrayLiteral("original")));
    QVERIFY(writeFile(sourceRoot + QStringLiteral("/top.txt"),
                      QByteArrayLiteral("top")));
    const QString archive = temp.filePath(QStringLiteral("test.zip"));
    {
        // Compressing the directory keeps the nested member name "src/sub/a.txt";
        // absolute file paths would be stored without their directories.
        // 压缩整个目录会保留嵌套成员名 "src/sub/a.txt"；
        // 绝对文件路径会被 7-Zip 去掉目录部分。
        ZipBackend writer;
        CompressOptions options;
        options.archivePath = archive;
        options.sourceFiles = {sourceRoot};
        bool finished = false;
        connect(&writer, &ArchiveBackend::operationFinished, &writer,
                [&finished](bool ok, const QString &) { finished = ok; });
        writer.compress(options);
        QVERIFY(finished);
    }

    // The preview cache mirrors the archive tree. / 预览缓存镜像压缩包目录树。
    const QString previewRoot = temp.filePath(QStringLiteral("preview"));
    QVERIFY(QDir().mkpath(previewRoot));
    {
        ZipBackend reader;
        reader.openArchive(archive);
        ExtractOptions options;
        options.destDir = previewRoot;
        options.overwrite = OverwritePolicy::Always;
        bool finished = false;
        connect(&reader, &ArchiveBackend::operationFinished, &reader,
                [&finished](bool ok, const QString &) { finished = ok; });
        reader.extract(options);
        QVERIFY(finished);
    }

    const QString memberPath = QStringLiteral("src/sub/a.txt");
    const QString localPath = previewRoot + QLatin1Char('/') + memberPath;
    QVERIFY2(QFileInfo::exists(localPath), qPrintable(localPath));

    ExternalEditManager &edits = ExternalEditManager::instance();
    edits.track(localPath, previewRoot, memberPath, archive,
                ArchiveFormat::Zip, QString(), QStringLiteral("UTF-8"));
    QVERIFY(edits.hasJob(localPath));

    // The editor saves new content. / 编辑器保存了新内容。
    QTest::qWait(5);
    QVERIFY(writeFile(localPath, QByteArrayLiteral("edited in the editor")));

    bool prompted = false;
    AddFilesOptions captured;
    bool saved = false;
    edits.setPromptHandler([&prompted](const ExternalEditManager::Job &) {
        prompted = true;
        return ExternalEditManager::Decision::Save;
    });
    edits.setSaveHandler([&](const ExternalEditManager::Job &job,
                             const AddFilesOptions &options) {
        captured = options;
        QCOMPARE(job.entryPath, memberPath);
        // The writing backend serves the already open archive, exactly like
        // ArchiveManager does.
        // 写回后端与 ArchiveManager 一样服务于已打开的压缩包。
        ZipBackend writer;
        writer.openArchive(archive);
        bool finished = false;
        connect(&writer, &ArchiveBackend::operationFinished, &writer,
                [&finished](bool ok, const QString &) { finished = ok; });
        writer.addFiles(options);
        saved = finished;
        edits.confirmSaved(job.localPath);
    });

    edits.checkNow();
    edits.checkNow();
    QVERIFY2(prompted, "the confirmation prompt was not shown");
    QVERIFY2(saved, "the write-back did not finish");

    QCOMPARE(captured.archivePath, archive);
    QCOMPARE(captured.sourceRoot, previewRoot);
    QCOMPARE(captured.sourceFiles, QStringList{memberPath});
    QCOMPARE(captured.nameEncoding, QStringLiteral("UTF-8"));

    // The archive still has two files; the edited one carries the new content.
    // 压缩包仍然只有两个文件；被编辑的那个带有新内容。
    ZipBackend reader;
    reader.openArchive(archive);
    QList<ArchiveEntry> entries;
    connect(&reader, &ArchiveBackend::fileListReady, &reader,
            [&entries](const QList<ArchiveEntry> &list) { entries = list; });
    reader.listContents();
    int fileCount = 0;
    for (const ArchiveEntry &entry : entries) {
        if (!entry.isDir) {
            ++fileCount;
        }
    }
    QCOMPARE(fileCount, 2);
    QCOMPARE(entries.size(), 4);

    const QString extractDir = temp.filePath(QStringLiteral("verify"));
    ExtractOptions extractOptions;
    extractOptions.destDir = extractDir;
    extractOptions.overwrite = OverwritePolicy::Always;
    bool extracted = false;
    connect(&reader, &ArchiveBackend::operationFinished, &reader,
            [&extracted](bool ok, const QString &) { extracted = ok; });
    reader.extract(extractOptions);
    QVERIFY(extracted);
    QCOMPARE(readFile(extractDir + QLatin1Char('/') + memberPath),
             QByteArrayLiteral("edited in the editor"));
    QCOMPARE(readFile(extractDir + QStringLiteral("/src/top.txt")),
             QByteArrayLiteral("top"));

    edits.forget(localPath);
}

// Answering "Don't Save" keeps the archive unchanged and stops asking.
// 选择“不保存”时压缩包保持不变，并且不再重复询问。
void TestExternalEdit::discardLeavesArchiveUntouched()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("test.zip"));
    const QString source = temp.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeFile(source, QByteArrayLiteral("original")));
    {
        ZipBackend writer;
        CompressOptions options;
        options.archivePath = archive;
        options.sourceFiles = {source};
        bool finished = false;
        connect(&writer, &ArchiveBackend::operationFinished, &writer,
                [&finished](bool ok, const QString &) { finished = ok; });
        writer.compress(options);
        QVERIFY(finished);
    }
    const QByteArray archiveBefore = readFile(archive);

    ExternalEditManager &edits = ExternalEditManager::instance();
    edits.track(source, temp.path(), QStringLiteral("a.txt"), archive,
                ArchiveFormat::Zip, QString(), QStringLiteral("UTF-8"));

    int prompts = 0;
    bool saveCalled = false;
    edits.setPromptHandler([&prompts](const ExternalEditManager::Job &) {
        ++prompts;
        return ExternalEditManager::Decision::Discard;
    });
    edits.setSaveHandler([&saveCalled](const ExternalEditManager::Job &,
                                       const AddFilesOptions &) {
        saveCalled = true;
    });

    QTest::qWait(5);
    QVERIFY(writeFile(source, QByteArrayLiteral("changed")));
    edits.checkNow();
    edits.checkNow();
    QCOMPARE(prompts, 1);
    QVERIFY(!saveCalled);
    QCOMPARE(readFile(archive), archiveBefore);

    // The baseline was refreshed: the same content does not ask again.
    // 基线已刷新：同样的内容不会再次询问。
    edits.checkNow();
    QCOMPARE(prompts, 1);

    edits.forget(source);
}

// An editor that rewrites identical bytes must not trigger a prompt.
// 重写相同字节的编辑器不应触发询问。
void TestExternalEdit::identicalRewriteIsIgnored()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString archive = temp.filePath(QStringLiteral("test.7z"));
    const QString file = temp.filePath(QStringLiteral("note.txt"));
    QVERIFY(writeFile(file, QByteArrayLiteral("same content")));

    ExternalEditManager &edits = ExternalEditManager::instance();
    edits.track(file, temp.path(), QStringLiteral("note.txt"), archive,
                ArchiveFormat::SevenZip, QString(), QStringLiteral("UTF-8"));

    int prompts = 0;
    edits.setPromptHandler([&prompts](const ExternalEditManager::Job &) {
        ++prompts;
        return ExternalEditManager::Decision::Discard;
    });

    QTest::qWait(5);
    QVERIFY(writeFile(file, QByteArrayLiteral("same content")));
    edits.checkNow();
    edits.checkNow();
    QCOMPARE(prompts, 0);
    edits.forget(file);
}

// Traversal and unrepresentable names are rejected by the staging helper.
// 暂存辅助函数会拒绝路径穿越与无法表示的名称。
void TestExternalEdit::unsafeStagePathIsRejected()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath(QStringLiteral("a.txt"));
    QVERIFY(writeFile(source, QByteArrayLiteral("data")));
    const QString staging = temp.filePath(QStringLiteral("staging"));
    QVERIFY(QDir().mkpath(staging));

    QString error;
    QVERIFY(!FileUtils::stageFileForArchiving(source, staging,
                                              QStringLiteral("../escape.txt"),
                                              QStringLiteral("UTF-8"), &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(temp.filePath(QStringLiteral("escape.txt"))));

    // UTF-8 names are staged under their normal name. / UTF-8 名称按原名暂存。
    QVERIFY(FileUtils::stageFileForArchiving(
        source, staging, QStringLiteral("sub/a.txt"), QStringLiteral("UTF-8"),
        &error));
    QCOMPARE(readFile(staging + QStringLiteral("/sub/a.txt")),
             QByteArrayLiteral("data"));
}

// Legacy code-page members are staged with their raw bytes so 7-Zip replaces
// the member instead of adding a second, UTF-8 named one.
// 旧代码页成员按原始字节暂存，使 7-Zip 替换该成员而不是追加一个 UTF-8 名称的副本。
void TestExternalEdit::legacyEncodingIsStagedWithRawName()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString source = temp.filePath(QStringLiteral("note.txt"));
    QVERIFY(writeFile(source, QByteArrayLiteral("edited")));
    const QString staging = temp.filePath(QStringLiteral("staging"));
    QVERIFY(QDir().mkpath(staging));

    const QString member = QStringLiteral("中文.txt");
    const QByteArray encoded = EncodingDetector::encode(member, QStringLiteral("GBK"));
    QVERIFY(!encoded.isEmpty());

    QString error;
    QVERIFY2(FileUtils::stageFileForArchiving(source, staging, member,
                                              QStringLiteral("GBK"), &error),
             qPrintable(error));

#ifndef Q_OS_WIN
    // The staged file must exist under the raw GBK bytes.  QString cannot
    // represent those bytes, so read them through POSIX helpers.
    // 暂存文件必须以原始 GBK 字节存在。QString 无法表示这些字节，
    // 因此通过 POSIX 接口读取。
    const QByteArray rawPath = QFile::encodeName(staging) + '/' + encoded;
    QCOMPARE(::access(rawPath.constData(), F_OK), 0);
    QFile rawReader;
    QByteArray content;
    const int fd = ::open(rawPath.constData(), O_RDONLY);
    QVERIFY(fd >= 0);
    char buffer[64];
    ssize_t bytesRead = 0;
    while ((bytesRead = ::read(fd, buffer, sizeof(buffer))) > 0) {
        content.append(buffer, static_cast<int>(bytesRead));
    }
    ::close(fd);
    QCOMPARE(content, QByteArrayLiteral("edited"));
    // QTemporaryDir cannot remove a raw-named file; do it explicitly.
    // QTemporaryDir 无法删除原始字节名文件，这里显式删除。
    QCOMPARE(::unlink(rawPath.constData()), 0);
#else
    QCOMPARE(readFile(staging + QLatin1Char('/') + member),
             QByteArrayLiteral("edited"));
#endif
}

// A GBK-named member is replaced in place after an external edit: the staging
// tree keeps the raw member name, so 7-Zip replaces that member instead of
// adding a UTF-8 named duplicate.
// 外部编辑后 GBK 名称的成员被就地替换：暂存目录保留原始成员名，因此 7-Zip
// 替换该成员，而不是追加一个 UTF-8 名称的副本。
void TestExternalEdit::legacyEncodingWriteBackReplacesMember()
{
#ifdef Q_OS_WIN
    QSKIP("legacy-codepage member names are normalised by 7z.exe on Windows");
#else
    QTemporaryDir temp;
    QVERIFY(temp.isValid());

    // "中文测试.txt" encoded as GBK, without the ZIP UTF-8 flag.
    // “中文测试.txt”的 GBK 编码，且不带 ZIP 的 UTF-8 标志。
    const QByteArray gbkName = QByteArray::fromHex("d6d0cec4b2e2cad42e747874");
    const QString member = EncodingDetector::decode(gbkName, QStringLiteral("GBK"));
    QVERIFY(!member.isEmpty());
    const QString archive = temp.filePath(QStringLiteral("legacy-gbk.zip"));
    QVERIFY(writeFile(archive, TestArchiveBuilder::makeStoredZip(
                                   gbkName, QByteArrayLiteral("legacy"))));

    // Extraction mirrors the archive and renames the member to UTF-8.
    // 解压镜像压缩包结构，并把成员名转换为 UTF-8。
    const QString previewRoot = temp.filePath(QStringLiteral("preview"));
    QVERIFY(QDir().mkpath(previewRoot));
    {
        ZipBackend reader;
        reader.openArchive(archive);
        // Listing detects the member-name encoding; extraction then renames the
        // legacy-codepage names to UTF-8, exactly like the preview flow.
        // 列表阶段检测成员名编码；解压随后把旧代码页名称转换为 UTF-8，
        // 与预览流程完全一致。
        bool listed = false;
        connect(&reader, &ArchiveBackend::fileListReady, &reader,
                [&listed](const QList<ArchiveEntry> &) { listed = true; });
        reader.listContents();
        QVERIFY(listed);
        ExtractOptions options;
        options.destDir = previewRoot;
        options.overwrite = OverwritePolicy::Always;
        bool finished = false;
        connect(&reader, &ArchiveBackend::operationFinished, &reader,
                [&finished](bool ok, const QString &) { finished = ok; });
        reader.extract(options);
        QVERIFY(finished);
    }
    const QString localPath = previewRoot + QLatin1Char('/') + member;
    QVERIFY2(QFileInfo::exists(localPath), qPrintable(localPath));

    ExternalEditManager &edits = ExternalEditManager::instance();
    edits.track(localPath, previewRoot, member, archive, ArchiveFormat::Zip,
                QString(), QStringLiteral("GBK"));

    bool prompted = false;
    bool saved = false;
    QString usedSourceRoot;
    edits.setPromptHandler([&prompted](const ExternalEditManager::Job &) {
        prompted = true;
        return ExternalEditManager::Decision::Save;
    });
    edits.setSaveHandler([&](const ExternalEditManager::Job &job,
                             const AddFilesOptions &options) {
        QCOMPARE(options.nameEncoding, QStringLiteral("GBK"));
        QVERIFY(!options.sourceRoot.isEmpty());
        usedSourceRoot = options.sourceRoot;
        ZipBackend writer;
        writer.openArchive(archive);
        bool finished = false;
        connect(&writer, &ArchiveBackend::operationFinished, &writer,
                [&finished](bool ok, const QString &) { finished = ok; });
        writer.addFiles(options);
        saved = finished;
        edits.confirmSaved(job.localPath);
    });

    QTest::qWait(5);
    QVERIFY(writeFile(localPath, QByteArrayLiteral("edited")));
    edits.checkNow();
    edits.checkNow();
    QVERIFY2(prompted, "the confirmation prompt was not shown");
    QVERIFY2(saved, "the write-back did not finish");
    // The legacy name forced a staging tree distinct from the preview root.
    // 旧式名称迫使使用与预览根目录不同的暂存目录树。
    QVERIFY(usedSourceRoot != previewRoot);

    // Still exactly one member, still GBK named, with the new content.
    // 仍然只有一个成员，名称仍是 GBK，内容已更新。
    ZipBackend reader;
    reader.openArchive(archive);
    QList<ArchiveEntry> entries;
    connect(&reader, &ArchiveBackend::fileListReady, &reader,
            [&entries](const QList<ArchiveEntry> &list) { entries = list; });
    reader.listContents();
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries.first().path, member);

    const QString extractDir = temp.filePath(QStringLiteral("verify"));
    ExtractOptions extractOptions;
    extractOptions.destDir = extractDir;
    extractOptions.overwrite = OverwritePolicy::Always;
    bool extracted = false;
    connect(&reader, &ArchiveBackend::operationFinished, &reader,
            [&extracted](bool ok, const QString &) { extracted = ok; });
    reader.extract(extractOptions);
    QVERIFY(extracted);
    QCOMPARE(readFile(extractDir + QLatin1Char('/') + member),
             QByteArrayLiteral("edited"));
    edits.forget(localPath);
#endif
}

QTEST_MAIN(TestExternalEdit)
#include "tst_externaledit.moc"
