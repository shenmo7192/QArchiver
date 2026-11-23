// QArchiver / 小Q解压
// Unit tests for filesystem, encoding and 7-Zip list parsing helpers.
// 文件系统、编码与 7-Zip 列表解析辅助函数的单元测试。

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <limits>

#include "backends/sevenzip/SevenZipListParser.h"
#include "utils/FileUtils.h"

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

QTEST_GUILESS_MAIN(TestUtils)

#include "tst_utils.moc"
