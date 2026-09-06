#include "ProcessHelper.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QTimer>
#include "FileUtils.h"

ProcessHelper::ProcessHelper(QObject *parent)
    : QObject(parent)
    , m_env(QProcessEnvironment::systemEnvironment())
{
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &ProcessHelper::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &ProcessHelper::onReadyReadStandardError);
    connect(m_process, &QProcess::finished, this, &ProcessHelper::onFinished);
}

ProcessHelper::~ProcessHelper()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
}

bool ProcessHelper::isToolAvailable(const QString &toolName)
{
    return !resolveToolPath(toolName).isEmpty();
}

QString ProcessHelper::resolveToolPath(const QString &toolName)
{
    return QStandardPaths::findExecutable(toolName);
}

void ProcessHelper::setEnvironment(const QProcessEnvironment &env)
{
    m_env = env;
}

void ProcessHelper::setWorkingDirectory(const QString &dir)
{
    m_workingDir = dir;
}

void ProcessHelper::start(const QString &program, const QStringList &args)
{
    m_exitCode = -1;
    m_process->setProcessEnvironment(m_env);
    if (!m_workingDir.isEmpty()) {
        m_process->setWorkingDirectory(m_workingDir);
    }
    m_process->start(program, args);
}

int ProcessHelper::executeSync(const QString &program, const QStringList &args, int timeoutMs)
{
    m_exitCode = -1;
    m_process->setProcessEnvironment(m_env);
    if (!m_workingDir.isEmpty()) {
        m_process->setWorkingDirectory(m_workingDir);
    }
    m_process->start(program, args);
    if (!m_process->waitForStarted(5000)) {
        return -1;
    }
    if (!m_process->waitForFinished(timeoutMs)) {
        // Timeout: kill the process
        m_process->kill();
        m_process->waitForFinished(3000);
        return -1;
    }
    return m_process->exitCode();
}

void ProcessHelper::cancel()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // Try terminate first, then kill after 3 seconds
        m_process->terminate();
        QTimer::singleShot(3000, m_process, &QProcess::kill);
    }
}

bool ProcessHelper::isRunning() const
{
    return m_process && m_process->state() != QProcess::NotRunning;
}

QString ProcessHelper::maskedCommand(const QString &program, const QStringList &args)
{
    QString cmd = program;
    for (const QString &arg : args) {
        cmd += ' ' + arg;
    }
    return FileUtils::maskPassword(cmd);
}

void ProcessHelper::onReadyReadStandardOutput()
{
    while (m_process->canReadLine()) {
        QByteArray line = m_process->readLine();
        QString str = QString::fromLocal8Bit(line).trimmed();
        if (!str.isEmpty()) {
            emit outputLine(str);
            emit progress(str);
        }
    }
}

void ProcessHelper::onReadyReadStandardError()
{
    while (m_process->canReadLine()) {
        QByteArray line = m_process->readLine();
        QString str = QString::fromLocal8Bit(line).trimmed();
        if (!str.isEmpty()) {
            emit errorLine(str);
            // stderr can also contain progress info for some tools
            emit progress(str);
        }
    }
}

void ProcessHelper::onFinished(int code, QProcess::ExitStatus status)
{
    m_exitCode = code;
    emit finished(code, status);
}
