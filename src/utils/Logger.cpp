#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QStandardPaths>
#include <QFileInfo>

Logger::Logger()
{
    // Enable debug logging via environment variable SAM_DEBUG=1
    if (qEnvironmentVariableIsSet("SAM_DEBUG")) {
        m_minLevel = Debug;
    }
}

Logger &Logger::instance()
{
    static Logger instance;
    return instance;
}

void Logger::setMinLevel(Level lvl)
{
    QMutexLocker locker(&m_mutex);
    m_minLevel = lvl;
}

void Logger::setLogFile(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_file.close();
    }
    // Ensure parent directory exists
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());

    m_logPath = path;
    m_file.setFileName(path);
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void Logger::log(Level lvl, const QString &tag, const QString &msg)
{
    QMutexLocker locker(&m_mutex);
    if (lvl < m_minLevel) {
        return;
    }

    const char *levelStr = "DEBUG";
    switch (lvl) {
    case Debug:   levelStr = "DEBUG";   break;
    case Info:    levelStr = "INFO";    break;
    case Warning: levelStr = "WARN";    break;
    case Error:   levelStr = "ERROR";   break;
    }

    const QString line = QString("[%1] [%2] [%3] %4\n")
        .arg(QDateTime::currentDateTime().toString(Qt::ISODate))
        .arg(levelStr)
        .arg(tag)
        .arg(msg);

    // Also output to stderr
    fputs(line.toLocal8Bit().constData(), stderr);

    if (m_file.isOpen()) {
        m_file.write(line.toLocal8Bit());
        m_file.flush();
        rotateIfNeeded();
    }
}

void Logger::rotateIfNeeded()
{
    if (m_file.size() < m_maxFileSize) {
        return;
    }
    // Rotate: current -> .1, drop old .1
    const QString oldPath = m_file.fileName() + ".1";
    m_file.close();
    QFile::remove(oldPath);
    QFile::rename(m_file.fileName(), oldPath);
    m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}
