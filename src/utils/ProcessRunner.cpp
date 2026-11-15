#include "ProcessRunner.h"

#include "EncodingDetector.h"

#include <QElapsedTimer>
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

} // namespace

bool ProcessRunner::isToolAvailable(const QString &toolName)
{
    return !resolveToolPath(toolName).isEmpty();
}

QString ProcessRunner::resolveToolPath(const QString &toolName)
{
    return QStandardPaths::findExecutable(toolName);
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
            result.stdErr += text;
            result.stdErr += QLatin1Char('\n');
        } else {
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
    readChannels();

    // Flush a trailing line without a newline.
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
