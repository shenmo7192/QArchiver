#ifndef PROCESSHELPER_H
#define PROCESSHELPER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QProcess>
#include <QProcessEnvironment>

// Wrapper around QProcess with timeout, env management, and progress output (design doc Infrastructure)
class ProcessHelper : public QObject {
    Q_OBJECT
public:
    explicit ProcessHelper(QObject *parent = nullptr);
    ~ProcessHelper();

    // Check whether a tool exists in PATH
    static bool isToolAvailable(const QString &toolName);
    // Resolve the full path of a tool
    static QString resolveToolPath(const QString &toolName);

    // Set environment variables for the process
    void setEnvironment(const QProcessEnvironment &env);
    // Set working directory
    void setWorkingDirectory(const QString &dir);

    // Start a command (non-blocking; connect to signals for output)
    void start(const QString &program, const QStringList &args);
    // Blocking run with timeout (ms); returns exit code; -1 on timeout/failure
    int executeSync(const QString &program, const QStringList &args, int timeoutMs = 30000);

    // Cancel the running process
    void cancel();
    // Whether the process is running
    bool isRunning() const;
    // Exit code of the last finished process
    int exitCode() const { return m_exitCode; }

    // Mask command arguments for logging (strip passwords)
    static QString maskedCommand(const QString &program, const QStringList &args);

signals:
    void outputLine(const QString &line);          // stdout line
    void errorLine(const QString &line);           // stderr line
    void progress(const QString &line);            // progress-relevant output
    void finished(int exitCode, QProcess::ExitStatus status);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onFinished(int code, QProcess::ExitStatus status);

private:
    QProcess *m_process = nullptr;
    QProcessEnvironment m_env;
    QString m_workingDir;
    int m_exitCode = -1;
};

#endif // PROCESSHELPER_H
