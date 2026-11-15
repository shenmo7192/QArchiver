#include <QtTest>

#include "backends/tar/TarOutputParser.h"
#include "utils/FileUtils.h"

class TestUtils : public QObject {
    Q_OBJECT

private slots:
    void normalizeEntryPath_data();
    void normalizeEntryPath();
    void traversalDetection();
    void tarParser();
};

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

void TestUtils::normalizeEntryPath()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(FileUtils::normalizeEntryPath(input), expected);
}

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

void TestUtils::tarParser()
{
    const ArchiveEntry dir = TarOutputParser::parseListLine(
        QStringLiteral("drwxr-xr-x user/group 0 2024-01-01 12:00 my dir/"));
    QVERIFY(dir.isDir);
    QCOMPARE(dir.path, QStringLiteral("my dir"));

    const ArchiveEntry file = TarOutputParser::parseListLine(
        QStringLiteral("-rw-r--r-- user/group 123 2024-01-01 12:00 my file.txt"));
    QVERIFY(!file.isDir);
    QCOMPARE(file.size, qint64(123));
    QCOMPARE(file.path, QStringLiteral("my file.txt"));
}

QTEST_GUILESS_MAIN(TestUtils)

#include "tst_utils.moc"
