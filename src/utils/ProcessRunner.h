#ifndef PROCESSRUNNER_H
#define PROCESSRUNNER_H

// QArchiver / 小Q解压
// Synchronous QProcess wrapper for the bundled 7-Zip helper.
// 用于随程序分发 7-Zip 辅助程序的同步 QProcess 封装。

#include <QByteArray>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

// Result of a synchronous external-tool invocation.
// 同步调用外部工具的结果。
struct ProcessResult {
    bool started = false;
    bool timedOut = false;
    bool cancelled = false;
    int exitCode = -1;
    QProcess::ExitStatus exitStatus = QProcess::NormalExit;
    QString stdOut;
    QString stdErr;
    // Raw bytes before decoding.  Callers can use EncodingDetector::detect()
    // on stdOutRaw to remember the archive's filename encoding (legacy ZIPs).
    // 解码前的原始字节。调用方可对 stdOutRaw 使用 EncodingDetector::detect()
    // 来记录压缩包的文件名编码（旧式 ZIP）。
    QByteArray stdOutRaw;
    QByteArray stdErrRaw;

    bool success() const
    {
        return started && !timedOut && !cancelled
            && exitStatus == QProcess::NormalExit && exitCode == 0;
    }
};

// Bounds for captured process output.  Line callbacks always receive every
// line; these limits only control the copies retained in ProcessResult.
// 进程输出的保留上限。行回调始终收到所有行；这些上限只控制
// ProcessResult 中保留的副本。
struct ProcessOutputOptions {
    // Do not accumulate the complete stdout stream.  A small sample can still
    // be retained for encoding detection via maxStdOutBytes/maxRawSampleBytes.
    // 不累计完整 stdout；仍可通过 maxStdOutBytes/maxRawSampleBytes
    // 保留用于编码检测的小样本。
    bool streamStdOut = false;
    // -1 = unlimited, 0 = discard, >0 = keep at most this many characters.
    // -1 = 不限制，0 = 丢弃，>0 = 最多保留这么多字符。
    qint64 maxStdOutBytes = -1;
    qint64 maxStdErrBytes = -1;
    // -1 = unlimited, 0 = discard, >0 = raw stdout sample limit in bytes.
    // -1 = 不限制，0 = 丢弃，>0 = 原始 stdout 样本字节上限。
    qint64 maxRawSampleBytes = -1;
};

// Central wrapper around QProcess for the bundled 7-Zip helper.
//
// It drains stdout and stderr concurrently, so verbose tools cannot deadlock
// on a full pipe, supports timeouts and cooperative cancellation, and decodes
// output using an explicit encoding hint (GBK/GB18030/Big5/Shift-JIS/UTF-8).
//
// resolveToolPath() prefers the bundled helper shipped in <exe dir>/7zip/
// (or the installed libexec directory) and only falls back to PATH, so the
// fallback backends keep working without a system-wide 7z installation.
// For "7z", candidates are additionally validated through --help by
// SevenZipTool before they are accepted.
//
// 内置 7-Zip 辅助程序的 QProcess 中心封装。
// 同时排空 stdout 与 stderr，避免输出较多的工具因管道写满而死锁；
// 支持超时、协作式取消，并按显式编码提示解码输出。
// resolveToolPath() 优先使用 <exe 目录>/7zip/ 或安装后的 libexec 目录中
// 随程序分发的辅助程序，最后才回退到 PATH，因此没有系统 7z 也能工作。
// 对 "7z"，候选还会由 SevenZipTool 通过 --help 校验后才会被接受。
class ProcessRunner {
public:
    using LineCallback = std::function<void(const QString &line, bool isError)>;

    static bool isToolAvailable(const QString &toolName);
    static QString resolveToolPath(const QString &toolName);

    // Program-relative and PATH candidates, in search order.  For 7z the
    // candidates are also validated as an actual 7-Zip CLI by SevenZipTool.
    // 按搜索顺序返回程序相关目录与 PATH 中的候选程序。
    // 对 7z，SevenZipTool 还会用 --help 校验其确实是 7-Zip CLI。
    static QStringList bundledDirectories();
    static QStringList bundledToolPaths(const QString &toolName);
    static QStringList systemToolPaths(const QString &toolName);

    // Mask "-p<password>" arguments for logging. / 日志输出时隐藏 "-p<密码>" 参数
    static QString maskedCommand(const QString &program, const QStringList &args);

    // Run @p program synchronously.
    // @param encodingHint  Empty -> auto-detect (UTF-8 first, then GB18030),
    //                      otherwise an iconv/QStringDecoder compatible name.
    // @param timeoutMs     0 means no timeout.
    // @param cancel        Optional flag polled while the process is running.
    // @param onLine        Optional callback invoked for every complete line.
    // 同步运行 @p program。
    // @param encodingHint  为空时自动检测（先 UTF-8，再 GB18030），
    //                      否则使用 iconv/QStringDecoder 兼容的编码名。
    // @param timeoutMs     0 表示不超时。
    // @param cancel        可选取消标志，进程运行期间轮询。
    // @param onLine        可选回调，每收到完整一行时调用。
    static ProcessResult run(const QString &program,
                             const QStringList &args,
                             const QString &encodingHint = QString(),
                             const QProcessEnvironment &env = QProcessEnvironment(),
                             const QString &workingDir = QString(),
                             int timeoutMs = 0,
                             const std::atomic_bool *cancel = nullptr,
                             const LineCallback &onLine = {},
                             const ProcessOutputOptions &outputOptions = {});
};

#endif // PROCESSRUNNER_H
