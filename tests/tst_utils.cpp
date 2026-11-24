// QArchiver / 小Q解压
// Unit tests for filesystem, encoding and 7-Zip list parsing helpers.
// 文件系统、编码与 7-Zip 列表解析辅助函数的单元测试。

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <limits>

#include "backends/sevenzip/SevenZipListParser.h"
#include "common/FormatRegistry.h"
#include "utils/FileUtils.h"
#include "utils/MimeAppsList.h"

class TestUtils : public QObject {
    Q_OBJECT

private slots:
    void normalizeEntryPath_data();
    void normalizeEntryPath();
    void traversalDetection();
    void sevenZipListParser();
    void checkedSizeAccumulation();
    void safeSymlinkTargets_data();
    void safeSymlinkTargets();
    void unsafeExtractedSymlinks();
    void parserPreservesSpacesAndLinks();
    void formatRegistrySuffixes();
    void archiveStripExtension();
    void mimeAppsListPreservesStandardFormat();
    void mimeAppsListCreatesMissingSections();
};

// Data rows for archive path normalization.
// 压缩包路径规范化的数据行。
void TestUtils::normalizeEntryPath_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("backslashes") << QStringLiteral("dir\\sub\\file.txt")
                                 << QStringLiteral("dir/sub/file.txt");
    QTest::newRow("drive letter") << QStringLiteral("C:\\dir\\file.txt")
                                  << QStringLiteral("dir/file.txt");
    QTest::newRow("leading slash") << QStringLiteral("/dir/file.txt")
                                   << QStringLiteral("dir/file.txt");
    QTest::newRow("dot dot") << QStringLiteral("dir/../file.txt")
                              << QStringLiteral("file.txt");
}

// Normalize separators, dot segments and drive prefixes.
// 规范化分隔符、点路径与盘符前缀。
void TestUtils::normalizeEntryPath()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(FileUtils::normalizeEntryPath(input), expected);
}

// Reject Zip Slip paths without false positives on ".." in names.
// 拒绝 Zip Slip 路径，同时避免把名称中的 ".." 误判。
void TestUtils::traversalDetection()
{
    QVERIFY(FileUtils::isPathTraversal(QStringLiteral("../etc/passwd"),
                                       QStringLiteral("/tmp/dest")));
    QVERIFY(FileUtils::isPathTraversal(QStringLiteral("a/../../etc/passwd"),
                                       QStringLiteral("/tmp/dest")));
    QVERIFY(!FileUtils::isPathTraversal(QStringLiteral("foo..bar"),
                                        QStringLiteral("/tmp/dest")));
    QVERIFY(!FileUtils::isPathTraversal(QStringLiteral("safe/file.txt"),
                                        QStringLiteral("/tmp/dest")));
}

// Parse directory, file and symlink blocks from 7z -slt output.
// 解析 7z -slt 输出中的目录、文件与符号链接块。
void TestUtils::sevenZipListParser()
{
    const QString output = QStringLiteral(
        "Path = archive.tar\n"
        "Type = tar\n"
        "----------\n"
        "Path = my dir\n"
        "Folder = +\n"
        "Size = 0\n"
        "Mode = drwxr-xr-x\n"
        "\n"
        "Path = my dir/my file.txt\n"
        "Folder = -\n"
        "Size = 123\n"
        "Mode = -rw-r--r--\n"
        "\n"
        "Path = my dir/link.txt\n"
        "Folder = -\n"
        "Size = 5\n"
        "Mode = lrwxrwxrwx\n"
        "Symbolic Link = my file.txt\n");

    const QList<ArchiveEntry> entries = SevenZipListParser::parse(output);
    QCOMPARE(entries.size(), 3);

    QVERIFY(entries.at(0).isDir);
    QCOMPARE(entries.at(0).path, QStringLiteral("my dir"));

    QVERIFY(!entries.at(1).isDir);
    QVERIFY(!entries.at(1).isSymLink);
    QCOMPARE(entries.at(1).size, qint64(123));
    QCOMPARE(entries.at(1).path, QStringLiteral("my dir/my file.txt"));

    QVERIFY(entries.at(2).isSymLink);
    QCOMPARE(entries.at(2).path, QStringLiteral("my dir/link.txt"));
}

// Size sums must reject overflow and negative sizes instead of wrapping.
// 大小累加必须拒绝溢出与负值，而不是回绕。
void TestUtils::checkedSizeAccumulation()
{
    ArchiveEntry maximum;
    maximum.path = QStringLiteral("maximum.bin");
    maximum.size = std::numeric_limits<qint64>::max();
    ArchiveEntry one;
    one.path = QStringLiteral("one.bin");
    one.size = 1;

    QCOMPARE(FileUtils::totalUnpackedSize({maximum, one}),
             std::numeric_limits<qint64>::max());

    qint64 checked = 0;
    QVERIFY(!FileUtils::totalUnpackedSizeChecked({maximum, one}, &checked));

    checked = 0;
    QVERIFY(FileUtils::totalUnpackedSizeChecked({maximum}, &checked));
    QCOMPARE(checked, std::numeric_limits<qint64>::max());
    QVERIFY(!FileUtils::addSizeChecked(checked, 1));

    ArchiveEntry negative;
    negative.path = QStringLiteral("negative.bin");
    negative.size = -1;
    checked = 0;
    QVERIFY(!FileUtils::totalUnpackedSizeChecked({negative}, &checked));
    QVERIFY(!FileUtils::addSizeChecked(checked, -1));
}

void TestUtils::safeSymlinkTargets_data()
{
    QTest::addColumn<QString>("entry");
    QTest::addColumn<QString>("target");
    QTest::addColumn<bool>("safe");

    QTest::newRow("sibling") << QStringLiteral("dir/link")
                             << QStringLiteral("../file.txt") << true;
    QTest::newRow("nested") << QStringLiteral("dir/link")
                            << QStringLiteral("sub/file.txt") << true;
    QTest::newRow("normalize") << QStringLiteral("dir/link")
                               << QStringLiteral("sub/../file.txt") << true;
    QTest::newRow("escape-root") << QStringLiteral("link")
                                 << QStringLiteral("../file.txt") << false;
    QTest::newRow("escape") << QStringLiteral("dir/link")
                            << QStringLiteral("../../file.txt") << false;
    QTest::newRow("absolute") << QStringLiteral("dir/link")
                              << QStringLiteral("/etc/passwd") << false;
    QTest::newRow("empty") << QStringLiteral("dir/link") << QString() << false;
}

void TestUtils::safeSymlinkTargets()
{
    QFETCH(QString, entry);
    QFETCH(QString, target);
    QFETCH(bool, safe);

    QCOMPARE(FileUtils::isSafeSymlinkTarget(entry, target), safe);
}

void TestUtils::unsafeExtractedSymlinks()
{
#ifdef Q_OS_WIN
    QSKIP("QFile::link creates .lnk shortcuts on Windows, not Unix symlinks");
#endif
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QVERIFY(QDir(temp.path()).mkpath(QStringLiteral("sub")));

    QFile inside(QDir(temp.path()).filePath(QStringLiteral("inside.txt")));
    QVERIFY(inside.open(QIODevice::WriteOnly));
    inside.write("inside");
    inside.close();

    const QString safeLink = QDir(temp.path()).filePath(QStringLiteral("sub/safe"));
    const QString unsafeLink = QDir(temp.path()).filePath(QStringLiteral("sub/unsafe"));
    QVERIFY(QFile::link(QStringLiteral("../inside.txt"), safeLink));
    QVERIFY(QFile::link(QStringLiteral("../../outside.txt"), unsafeLink));

    const QStringList unsafe = FileUtils::unsafeExtractedSymlinks(temp.path());
    QCOMPARE(unsafe.size(), 1);
    QCOMPARE(unsafe.first(), QStringLiteral("sub/unsafe"));
}

// Streaming parser keeps member-name spaces and captures symlink/hardlink data.
// 流式解析器保留成员名空格，并捕获符号链接/硬链接数据。
void TestUtils::parserPreservesSpacesAndLinks()
{
    SevenZipListParser parser;
    parser.feedLine(QStringLiteral("Path =  leading.txt "));
    parser.feedLine(QStringLiteral("Folder = -"));
    parser.feedLine(QStringLiteral("Size = 3"));
    parser.feedLine(QStringLiteral("Hard Link = target.txt"));
    parser.feedLine(QStringLiteral("Path = dir/link"));
    parser.feedLine(QStringLiteral("Folder = -"));
    parser.feedLine(QStringLiteral("Symbolic Link = ../target.txt"));

    const QList<ArchiveEntry> entries = parser.takeEntries();
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries.at(0).path, QStringLiteral(" leading.txt "));
    QVERIFY(entries.at(0).isHardLink);
    QCOMPARE(entries.at(0).hardLinkTarget, QStringLiteral("target.txt"));
    QVERIFY(entries.at(1).isSymLink);
    QCOMPARE(entries.at(1).linkTarget, QStringLiteral("../target.txt"));
}

// Suffix detection must not confuse ordinary .r?? files with RAR volumes.
// 后缀识别不能把普通 .r?? 文件误判为 RAR 分卷。
void TestUtils::formatRegistrySuffixes()
{
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("notes.rtf")),
             ArchiveFormat::Unknown);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("package.rpm")),
             ArchiveFormat::Unknown);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("readme.rst")),
             ArchiveFormat::Unknown);
    QVERIFY(!FormatRegistry::isArchivePath(QStringLiteral("notes.rtf")));

    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.r00")),
             ArchiveFormat::Rar4);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.r01")),
             ArchiveFormat::Rar4);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.rar")),
             ArchiveFormat::Rar5);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.part2.rar")),
             ArchiveFormat::Rar5);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.zip.001")),
             ArchiveFormat::Zip);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.7z.001")),
             ArchiveFormat::SevenZip);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.tar.zst")),
             ArchiveFormat::TarZst);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.tzst")),
             ArchiveFormat::TarZst);
    QCOMPARE(FormatRegistry::formatFromSuffix(QStringLiteral("data.zst")),
             ArchiveFormat::Zstd);

    QVERIFY(FormatRegistry::isArchivePath(QStringLiteral("data.tar.zst")));
    QVERIFY(FormatRegistry::isArchivePath(QStringLiteral("data.zst")));

    QVERIFY(FormatRegistry::isSplitVolume(QStringLiteral("data.r00")));
    QVERIFY(FormatRegistry::isSplitVolume(QStringLiteral("data.part1.rar")));
    QVERIFY(FormatRegistry::isSplitVolume(QStringLiteral("data.zip.001")));
    QVERIFY(FormatRegistry::isArchivePath(QStringLiteral("data.r00")));
    QVERIFY(FormatRegistry::isArchivePath(QStringLiteral("data.part1.rar")));
}

// One-click extraction uses the archive base name, not a volume suffix.
// 一键解压使用压缩包基名，而不是分卷后缀。
void TestUtils::archiveStripExtension()
{
    QCOMPARE(FormatRegistry::stripArchiveExtension(
                 QStringLiteral("data.part1.rar")),
             QStringLiteral("data"));
    QCOMPARE(FormatRegistry::stripArchiveExtension(
                 QStringLiteral("data.part12.rar")),
             QStringLiteral("data"));
    QCOMPARE(FormatRegistry::stripArchiveExtension(
                 QStringLiteral("data.7z.001")),
             QStringLiteral("data"));
    QCOMPARE(FormatRegistry::stripArchiveExtension(
                 QStringLiteral("data.r00")),
             QStringLiteral("data"));
    QCOMPARE(FormatRegistry::stripArchiveExtension(
                 QStringLiteral("data.tar.gz")),
             QStringLiteral("data"));
    QCOMPARE(FormatRegistry::stripArchiveExtension(
                 QStringLiteral("plain.txt")),
             QStringLiteral("plain.txt"));
}

// Standard mimeapps.list syntax must survive a read/modify/write cycle.
// 标准 mimeapps.list 语法在读写后必须保持不变。
void TestUtils::mimeAppsListPreservesStandardFormat()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath(QStringLiteral("mimeapps.list"));

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArrayLiteral(
        "# user overrides\n"
        "[Default Applications]\n"
        "application/zip=file-roller.desktop;\n"
        "text/plain=org.gnome.TextEditor.desktop\n"
        "\n"
        "[Added Associations]\n"
        "application/zip=file-roller.desktop;\n"
        "\n"
        "[Removed Associations]\n"
        "image/png=example.desktop;\n"));
    file.close();

    QString error;
    QVERIFY2(MimeAppsList::setAssociation(
                 path, QStringLiteral("application/zip"),
                 QStringLiteral("qarchiver.desktop"), true, &error),
             qPrintable(error));

    QCOMPARE(MimeAppsList::desktopIds(
                 path, QStringLiteral("Default Applications"),
                 QStringLiteral("application/zip")),
             (QStringList{QStringLiteral("qarchiver.desktop"),
                          QStringLiteral("file-roller.desktop")}));
    QCOMPARE(MimeAppsList::desktopIds(
                 path, QStringLiteral("Added Associations"),
                 QStringLiteral("application/zip")),
             (QStringList{QStringLiteral("qarchiver.desktop"),
                          QStringLiteral("file-roller.desktop")}));
    QVERIFY(MimeAppsList::hasDesktopId(
        path, QStringLiteral("Default Applications"),
        QStringLiteral("application/zip"), QStringLiteral("qarchiver.desktop")));

    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    file.close();
    QVERIFY(content.contains(QStringLiteral("[Default Applications]")));
    QVERIFY(content.contains(QStringLiteral("[Added Associations]")));
    QVERIFY(content.contains(QStringLiteral("[Removed Associations]")));
    QVERIFY(content.contains(QStringLiteral("image/png=example.desktop;")));
    QVERIFY(content.contains(QStringLiteral(
        "application/zip=qarchiver.desktop;file-roller.desktop;")));
    QVERIFY(!content.contains(QStringLiteral("%20")));
    QVERIFY(!content.contains(QStringLiteral("application\\zip")));

    QVERIFY2(MimeAppsList::setAssociation(
                 path, QStringLiteral("application/zip"),
                 QStringLiteral("qarchiver.desktop"), false, &error),
             qPrintable(error));
    QCOMPARE(MimeAppsList::desktopIds(
                 path, QStringLiteral("Default Applications"),
                 QStringLiteral("application/zip")),
             QStringList{QStringLiteral("file-roller.desktop")});
}

// Missing groups are appended using the standard freedesktop headers.
// 缺失的节必须使用标准 freedesktop 标题追加。
void TestUtils::mimeAppsListCreatesMissingSections()
{
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = QDir(temp.path()).filePath(QStringLiteral("mimeapps.list"));

    QString error;
    QVERIFY2(MimeAppsList::setAssociation(
                 path, QStringLiteral("application/x-7z-compressed"),
                 QStringLiteral("qarchiver.desktop"), true, &error),
             qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QString content = QString::fromUtf8(file.readAll());
    QVERIFY(content.contains(QStringLiteral("[Default Applications]")));
    QVERIFY(content.contains(QStringLiteral("[Added Associations]")));
    QVERIFY(content.contains(QStringLiteral(
        "application/x-7z-compressed=qarchiver.desktop;")));

    // Removing from a missing file must not create an empty mimeapps.list.
    // 从不存在的文件移除关联时，不应创建空的 mimeapps.list。
    const QString missing =
        QDir(temp.path()).filePath(QStringLiteral("missing.list"));
    QVERIFY(MimeAppsList::setAssociation(
        missing, QStringLiteral("application/zip"),
        QStringLiteral("qarchiver.desktop"), false, &error));
    QVERIFY(!QFileInfo::exists(missing));
}

QTEST_GUILESS_MAIN(TestUtils)

#include "tst_utils.moc"
