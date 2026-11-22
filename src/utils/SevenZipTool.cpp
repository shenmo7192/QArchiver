// QArchiver / 小Q解压
// 7-Zip CLI discovery, --help validation and RAR codec probing.
// 7-Zip CLI 发现、--help 校验与 RAR 编解码器探测。

#include "SevenZipTool.h"

#include "Logger.h"
#include "ProcessRunner.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <mutex>

namespace {

constexpr int kProbeTimeoutMs = 10000;

std::mutex g_cacheMutex;
bool g_cacheValid = false;
SevenZipToolInfo g_cachedInfo;

SevenZipToolInfo detectUncached()
{
    SevenZipToolInfo info;
    info.bundledDirectories = SevenZipTool::bundledDirectories();

    // Explicit override wins and is trusted like before.  It is primarily a
    // test/packager escape hatch, not a normal user configuration.
    // 显式覆盖优先且沿用原有的“信任”语义，主要供测试/打包使用。
    const QString overridePath = qEnvironmentVariable("QARCHIVER_7Z_PATH");
    if (!overridePath.isEmpty()) {
        const QFileInfo infoFile(overridePath);
        if (infoFile.exists() && infoFile.isFile() && infoFile.isExecutable()) {
            info.source = SevenZipToolInfo::Source::Override;
            info.path = QDir::cleanPath(infoFile.absoluteFilePath());
            info.supportsRar = SevenZipTool::supportsRar(info.path);
        }
        return info;
    }

    // 1. Program-relative helper (build tree, install libexec, manual copy).
    // 1. 程序相关目录中的辅助程序（构建树、安装后的 libexec、手动复制）。
    const QStringList bundled = ProcessRunner::bundledToolPaths(QStringLiteral("7z"));
    for (const QString &candidate : bundled) {
        const ProcessResult help = ProcessRunner::run(
            candidate, {QStringLiteral("--help")}, QString(),
            QProcessEnvironment(), QString(), kProbeTimeoutMs);
        if (help.started
            && SevenZipTool::isSevenZipHelpText(help.stdOut + QLatin1Char('\n')
                                                + help.stdErr)) {
            info.source = SevenZipToolInfo::Source::Bundled;
            info.path = candidate;
            info.supportsRar = SevenZipTool::supportsRar(candidate);
            LOG_INFO("SevenZipTool", "Using bundled 7-Zip: %s (RAR=%s)",
                     qPrintable(info.path), info.supportsRar ? "yes" : "no");
            return info;
        }
        LOG_WARN("SevenZipTool",
                 "Ignoring candidate that is not a 7-Zip CLI: %s",
                 qPrintable(candidate));
    }

    // 2. System PATH fallback.
    // 2. 系统 PATH 回退。
    const QStringList system = ProcessRunner::systemToolPaths(QStringLiteral("7z"));
    for (const QString &candidate : system) {
        const ProcessResult help = ProcessRunner::run(
            candidate, {QStringLiteral("--help")}, QString(),
            QProcessEnvironment(), QString(), kProbeTimeoutMs);
        if (help.started
            && SevenZipTool::isSevenZipHelpText(help.stdOut + QLatin1Char('\n')
                                                + help.stdErr)) {
            info.source = SevenZipToolInfo::Source::System;
            info.path = candidate;
            info.supportsRar = SevenZipTool::supportsRar(candidate);
            LOG_WARN("SevenZipTool",
                     "Bundled 7-Zip missing; using system 7-Zip: %s (RAR=%s)",
                     qPrintable(info.path), info.supportsRar ? "yes" : "no");
            return info;
        }
        LOG_WARN("SevenZipTool",
                 "Ignoring system candidate that is not a 7-Zip CLI: %s",
                 qPrintable(candidate));
    }

    LOG_ERROR_S("SevenZipTool", "No usable 7-Zip command-line tool found");
    return info;
}

} // namespace

namespace SevenZipTool {

SevenZipToolInfo detect()
{
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    if (!g_cacheValid) {
        g_cachedInfo = detectUncached();
        g_cacheValid = true;
    }
    return g_cachedInfo;
}

QString resolvePath()
{
    return detect().path;
}

void resetCache()
{
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_cacheValid = false;
    g_cachedInfo = SevenZipToolInfo();
}

QStringList bundledDirectories()
{
    QStringList dirs;
    const QString appDir = QCoreApplication::applicationDirPath();
    if (appDir.isEmpty()) {
        return dirs;
    }

    const QDir base(appDir);
    dirs << base.filePath(QStringLiteral("7zip"));
    dirs << appDir;
    dirs << QDir::cleanPath(base.filePath(QStringLiteral("../lib/qarchiver/7zip")));
    dirs << QDir::cleanPath(base.filePath(QStringLiteral("../libexec/qarchiver/7zip")));
    dirs << QDir::cleanPath(base.filePath(QStringLiteral("../lib/qarchiver")));
    dirs << QDir::cleanPath(base.filePath(QStringLiteral("../libexec/qarchiver")));
    return dirs;
}

bool isSevenZipHelpText(const QString &output)
{
    if (output.contains(QStringLiteral("7-Zip"), Qt::CaseInsensitive)) {
        return true;
    }
    // Localized help may not keep the English "7-Zip" banner, but the usage
    // line still names the executable family.
    // 本地化帮助可能不保留英文 "7-Zip" 标题，但用法行仍会写出程序名。
    static const QRegularExpression usageRe(
        QStringLiteral("\\bUsage:\\s*7z(?:z|a|zs)?\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return usageRe.match(output).hasMatch();
}

bool codecsContainRar(const QString &output)
{
    const int codecsPos = output.indexOf(QStringLiteral("Codecs:"),
                                        0, Qt::CaseInsensitive);
    if (codecsPos < 0) {
        return false;
    }
    const QString codecs = output.mid(codecsPos);
    static const QRegularExpression rarRe(
        QStringLiteral("\\bRar\\d*\\b"),
        QRegularExpression::CaseInsensitiveOption);
    return rarRe.match(codecs).hasMatch();
}

bool supportsRar(const QString &executable)
{
    const ProcessResult result = ProcessRunner::run(
        executable, {QStringLiteral("i")}, QString(),
        QProcessEnvironment(), QString(), kProbeTimeoutMs);
    if (!result.started) {
        return false;
    }
    return codecsContainRar(result.stdOut + QLatin1Char('\n') + result.stdErr);
}

} // namespace SevenZipTool
