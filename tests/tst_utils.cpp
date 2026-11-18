// QArchiver / 小Q解压
// Unit tests for filesystem, encoding and 7-Zip list parsing helpers.
// 文件系统、编码与 7-Zip 列表解析辅助函数的单元测试。

#include <QtTest>

#include "backends/sevenzip/SevenZipListParser.h"
#include "utils/FileUtils.h"

class TestUtils : public QObject {
    Q_OBJECT

private slots:
    void normalizeEntryPath_data();
    void normalizeEntryPath();
    void traversalDetection();
    void sevenZipListParser();
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

QTEST_GUILESS_MAIN(TestUtils)

#include "tst_utils.moc"
