// QArchiver / 小Q解压
// Tool discovery, process I/O draining, cancellation and password masking.
// 工具查找、进程 I/O 排空、取消与密码脱敏。

#include "ProcessRunner.h"

#include "EncodingDetector.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

QString decodeLine(const QByteArray &bytes, const QString &encodingHint)
{
    if (encodingHint.isEmpty()) {
        return EncodingDetector::decode(bytes);
    }
    if (encodingHint.compare(QStringLiteral("UTF-8"), Qt::CaseInsensitive) == 0) {
        return QString::fromUtf8(bytes);
    }
    return EncodingDetector::decode(bytes, encodingHint);
}

bool isExecutableFile(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return false;
    }
#ifdef Q_OS_WIN
    // Windows has no Unix execute bit; the known candidate names are enough.
    // Windows 没有 Unix 可执行位；已知候选文件名足以判断。
    return true;
#else
    return info.isExecutable();
#endif
}

QString environmentOverrideName(const QString &toolName)
{
    QString upper = toolName.toUpper();
    for (QChar &ch : upper) {
        if (!ch.isLetterOrNumber()) {
            ch = QLatin1Char('_');
        }
    }
    return QStringLiteral("QARCHIVER_%1_PATH").arg(upper);
}

QStringList toolCandidates(const QString &toolName)
{
    QStringList candidates;
    if (toolName == QLatin1String("7z")) {
        // 7zzs is the statically linked Linux build; 7z.exe needs the matching
        // 7z.dll next to it on Windows.
        // 7zzs 是 Linux 静态链接版本；Windows 的 7z.exe 需要同目录下匹配的 7z.dll。
        candidates << QStringLiteral("7zzs")
                   << QStringLiteral("7zz")
                   << QStringLiteral("7z")
                   << QStringLiteral("7za")
                   << QStringLiteral("qarchiver-7z");
#ifdef Q_OS_WIN
        candidates << QStringLiteral("7z.exe")
                   << QStringLiteral("7za.exe")
                   << QStringLiteral("qarchiver-7z.exe");
#endif
    } else {
        candidates << toolName;
#ifdef Q_OS_WIN
        if (!toolName.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
            candidates << toolName + QStringLiteral(".exe");
        }
#endif
    }
    return candidates;
}

QStringList bundledSearchDirectories()
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

} // namespace

bool ProcessRunner::isToolAvailable(const QString &toolName)
{
    return !resolveToolPath(toolName).isEmpty();
}

QString ProcessRunner::resolveToolPath(const QString &toolName)
{
    // 1. Explicit override, primarily used by the test suite and packagers.
    // 1. 显式覆盖，主要供测试套件和打包者使用。
    const QByteArray overrideName = environmentOverrideName(toolName).toUtf8();
    const QString overridePath = qEnvironmentVariable(overrideName.constData());
    if (!overridePath.isEmpty()) {
        if (!isExecutableFile(overridePath)) {
            return QString();
        }
        return QDir::cleanPath(QFileInfo(overridePath).absoluteFilePath());
    }

    const QStringList candidates = toolCandidates(toolName);

    // 2. Bundled helper next to the executable / in libexec.
    // 2. 可执行文件旁或 libexec 中随程序分发的辅助程序。
    const QStringList bundledDirs = bundledSearchDirectories();
    for (const QString &dir : bundledDirs) {
        const QDir candidateDir(dir);
        for (const QString &candidate : candidates) {
            const QString path = candidateDir.filePath(candidate);
            if (isExecutableFile(path)) {
                return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
            }
        }
    }

    // 3. System PATH as the final fallback. / 3. 最后回退到系统 PATH。
    for (const QString &candidate : candidates) {
        const QString path = QStandardPaths::findExecutable(candidate);
        if (!path.isEmpty()) {
            return path;
        }
    }
    return QString();
}

QString ProcessRunner::maskedCommand(const QString &program, const QStringList &args)
{
    QStringList masked;
    masked.reserve(args.size());
    for (const QString &arg : args) {
        if (arg.startsWith(QStringLiteral("-p")) && arg.size() > 2) {
            masked << QStringLiteral("-p***");
        } else {
            masked << arg;
        }
    }
    return program + QLatin1Char(' ') + masked.join(QLatin1Char(' '));
}

ProcessResult ProcessRunner::run(const QString &program,
                                 const QStringList &args,
                                 const QString &encodingHint,
                                 const QProcessEnvironment &env,
                                 const QString &workingDir,
                                 int timeoutMs,
                                 const std::atomic_bool *cancel,
                                 const LineCallback &onLine)
{
    ProcessResult result;

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    if (!env.isEmpty()) {
        proc.setProcessEnvironment(env);
    }
    if (!workingDir.isEmpty()) {
        proc.setWorkingDirectory(workingDir);
    }

    proc.start(program, args);
    if (!proc.waitForStarted(5000)) {
        result.started = false;
        result.stdErr = proc.errorString();
        return result;
    }
    result.started = true;

    QByteArray outBuffer;
    QByteArray errBuffer;

    auto appendLine = [&](const QByteArray &raw, bool isError) {
        QByteArray line = raw;
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        const QString text = decodeLine(line, encodingHint);
        if (text.isEmpty()) {
            return;
        }
        if (isError) {
            result.stdErrRaw += line;
            result.stdErrRaw += '\n';
            result.stdErr += text;
            result.stdErr += QLatin1Char('\n');
        } else {
            result.stdOutRaw += line;
            result.stdOutRaw += '\n';
            result.stdOut += text;
            result.stdOut += QLatin1Char('\n');
        }
        if (onLine) {
            onLine(text, isError);
        }
    };

    auto drainCompleteLines = [&](QByteArray &buffer, bool isError) {
        int newline = -1;
        while ((newline = buffer.indexOf('\n')) >= 0) {
            const QByteArray line = buffer.left(newline);
            buffer.remove(0, newline + 1);
            appendLine(line, isError);
        }
    };

    auto readChannels = [&]() {
        outBuffer += proc.readAllStandardOutput();
        errBuffer += proc.readAllStandardError();
        drainCompleteLines(outBuffer, false);
        drainCompleteLines(errBuffer, true);
    };

    QElapsedTimer timer;
    timer.start();

    while (proc.state() != QProcess::NotRunning) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            result.cancelled = true;
            proc.terminate();
            if (!proc.waitForFinished(2000)) {
                proc.kill();
                proc.waitForFinished(2000);
            }
            break;
        }
        if (timeoutMs > 0 && timer.elapsed() > timeoutMs) {
            result.timedOut = true;
            proc.kill();
            proc.waitForFinished(2000);
            break;
        }

        proc.waitForReadyRead(200);
        readChannels();
    }

    // Collect whatever is still buffered after the process exited.
    // 进程退出后收集仍留在缓冲区中的数据。
    readChannels();

    // Flush a trailing line without a newline. / 输出末尾没有换行时也刷新该行。
    if (!outBuffer.isEmpty()) {
        appendLine(outBuffer, false);
        outBuffer.clear();
    }
    if (!errBuffer.isEmpty()) {
        appendLine(errBuffer, true);
        errBuffer.clear();
    }

    result.exitCode = proc.exitCode();
    result.exitStatus = proc.exitStatus();
    return result;
}
