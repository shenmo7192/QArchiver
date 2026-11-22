// QArchiver / 小Q解压
// Tests for 7-Zip CLI discovery, --help validation and RAR codec probing.
// 7-Zip CLI 发现、--help 校验与 RAR 编解码器探测测试。

#include <QtTest>

#include <QFileInfo>

#include "utils/ProcessRunner.h"
#include "utils/SevenZipTool.h"

class TestToolchain : public QObject {
    Q_OBJECT

private slots:
    void helpTextValidation();
    void rarCodecParsing();
    void overrideDetection();
    void systemFallbackDetection();
    void invalidOverride();
};

// `--help` must identify the binary as 7-Zip, not just any executable.
// `--help` 必须表明这是 7-Zip，而不是任意可执行文件。
void TestToolchain::helpTextValidation()
{
    QVERIFY(SevenZipTool::isSevenZipHelpText(
        QStringLiteral("7-Zip (z) 26.06 (x64) : Copyright (c) 1999-2026")));
    QVERIFY(SevenZipTool::isSevenZipHelpText(
        QStringLiteral("Usage: 7z <command> [<switches>...]")));
    QVERIFY(!SevenZipTool::isSevenZipHelpText(
        QStringLiteral("Usage: unrelated-tool --help")));
}

// Only the Codecs section counts; "Rar" in the Formats section is not enough.
// 只检查 Codecs 区段；仅出现在 Formats 区段不算。
void TestToolchain::rarCodecParsing()
{
    const QString withRar = QStringLiteral(
        "Formats:\n"
        " ...F..................  Rar      rar r00       R a r ! 1A 07 00\n"
        "Codecs:\n"
        "     D     40301 Rar1\n"
        "     D     40305 Rar5\n");
    QVERIFY(SevenZipTool::codecsContainRar(withRar));

    const QString withoutRar = QStringLiteral(
        "Codecs:\n"
        "    ED     40108 Deflate\n"
        "    ED        21 LZMA2\n");
    QVERIFY(!SevenZipTool::codecsContainRar(withoutRar));

    const QString formatsOnly = QStringLiteral(
        "Formats:\n"
        " ...F..................  Rar      rar r00\n");
    QVERIFY(!SevenZipTool::codecsContainRar(formatsOnly));
}

// The explicit test override is reported as such and resolves consistently.
// 测试用显式覆盖应报告为 Override，并与 resolveToolPath() 一致。
void TestToolchain::overrideDetection()
{
    const QString overridePath = qEnvironmentVariable("QARCHIVER_7Z_PATH");
    if (overridePath.isEmpty()) {
        QSKIP("QARCHIVER_7Z_PATH is not set");
    }

    SevenZipTool::resetCache();
    const SevenZipToolInfo info = SevenZipTool::detect();
    QCOMPARE(info.source, SevenZipToolInfo::Source::Override);
    QVERIFY(info.available());
    QCOMPARE(info.path, QFileInfo(overridePath).absoluteFilePath());
    QVERIFY(QFileInfo::exists(info.path));
    QVERIFY(info.supportsRar);
    QCOMPARE(ProcessRunner::resolveToolPath(QStringLiteral("7z")), info.path);
}

// Without an override, a valid system 7-Zip is accepted as the fallback.
// 没有显式覆盖时，有效的系统 7-Zip 会被选为兜底工具。
void TestToolchain::systemFallbackDetection()
{
    if (!qEnvironmentVariableIsEmpty("QARCHIVER_7Z_PATH")) {
        QSKIP("the test environment uses an explicit 7z override");
    }
    if (ProcessRunner::systemToolPaths(QStringLiteral("7z")).isEmpty()) {
        QSKIP("no system 7z candidate is installed");
    }

    SevenZipTool::resetCache();
    const SevenZipToolInfo info = SevenZipTool::detect();
    QCOMPARE(info.source, SevenZipToolInfo::Source::System);
    QVERIFY(info.available());
    QVERIFY(QFileInfo::exists(info.path));
    QCOMPARE(ProcessRunner::resolveToolPath(QStringLiteral("7z")), info.path);
}

// An invalid explicit override must not silently resolve to another binary.
// 无效的显式覆盖不得悄悄解析到其他二进制。
void TestToolchain::invalidOverride()
{
    const QByteArray saved = qgetenv("QARCHIVER_7Z_PATH");
    const QByteArray invalid = QByteArrayLiteral("/nonexistent/qarchiver-7z");
    qputenv("QARCHIVER_7Z_PATH", invalid);
    SevenZipTool::resetCache();

    const SevenZipToolInfo info = SevenZipTool::detect();
    QCOMPARE(info.source, SevenZipToolInfo::Source::None);
    QVERIFY(!info.available());
    QVERIFY(ProcessRunner::resolveToolPath(QStringLiteral("7z")).isEmpty());

    if (saved.isEmpty()) {
        qunsetenv("QARCHIVER_7Z_PATH");
    } else {
        qputenv("QARCHIVER_7Z_PATH", saved);
    }
    SevenZipTool::resetCache();
}

QTEST_GUILESS_MAIN(TestToolchain)

#include "tst_toolchain.moc"
