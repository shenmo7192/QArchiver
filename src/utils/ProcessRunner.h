#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

// Result of a synchronous external-tool invocation.
struct ProcessResult {
    bool started = false;
    bool timedOut = false;
    bool cancelled = false;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    QString stdOut;
    QString stdErr;

    bool success() const
    {
        return started && !timedOut && !cancelled
            && exitStatus == QProcess::NormalExit && exitCode == 0;
    }
};

// Central wrapper around QProcess for every external tool (tar/7z/unrar).
//
// It drains stdout and stderr concurrently, so verbose tools cannot deadlock
// on a full pipe, supports timeouts and cooperative cancellation, and decodes
// output using an explicit encoding hint (GBK/GB18030/Big5/Shift-JIS/UTF-8).
class ProcessRunner {
public:
    using LineCallback = std::function<void(const QString &line, bool isError)>;

    static bool isToolAvailable(const QString &toolName);
    static QString resolveToolPath(const QString &toolName);

    // Mask "-p<password>" arguments for logging.
    static QString maskedCommand(const QString &program, const QStringList &args);

    // Run @p program synchronously.
    // @param encodingHint  Empty -> auto-detect (UTF-8 first, then GB18030),
    //                      otherwise an iconv/QStringDecoder compatible name.
    // @param timeoutMs     0 means no timeout.
    // @param cancel        Optional flag polled while the process is running.
    // @param onLine        Optional callback invoked for every complete line.
    static ProcessResult run(const QString &program,
                             const QStringList &args,
                             const QString &encodingHint = QString(),
                             const QProcessEnvironment &env = QProcessEnvironment(),
                             const QString &workingDir = QString(),
                             int timeoutMs = 0,
                             const std::atomic_bool *cancel = nullptr,
                             const LineCallback &onLine = {});
};

#endif // PROCESSRUNNER_H
