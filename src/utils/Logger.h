#ifndef LOGGER_H
#define LOGGER_H

// QArchiver / 小Q解压
// Thread-safe logging facade with a rotating file sink.
// 线程安全的日志门面，支持滚动日志文件。

#include <QString>
#include <QMutex>
#include <QFile>

// Thread-safe logging facade (design doc 4.7).
// 线程安全的日志门面（设计文档 4.7）。
class Logger {
public:
    enum Level { Debug, Info, Warning, Error };

    static Logger &instance();

    void setMinLevel(Level lvl);
    Level minLevel() const { return m_minLevel; }

    void setLogFile(const QString &path);   // File sink / 文件输出
    QString logFilePath() const { return m_logPath; }
    void log(Level lvl, const QString &tag, const QString &msg);

private:
    Logger();
    Q_DISABLE_COPY(Logger)

    void rotateIfNeeded();   // Roll log file at 5MB / 日志达到 5MB 时轮转

    QMutex   m_mutex;
    QFile    m_file;
    QString  m_logPath;
    Level    m_minLevel = Info;
    qint64   m_maxFileSize = 5 * 1024 * 1024; // 5 MB
};

// Convenience macros for logging / 日志便捷宏
#define LOG_DEBUG(tag, ...) Logger::instance().log(Logger::Debug,   tag, QString::asprintf(__VA_ARGS__))
#define LOG_INFO(tag, ...)  Logger::instance().log(Logger::Info,    tag, QString::asprintf(__VA_ARGS__))
#define LOG_WARN(tag, ...)  Logger::instance().log(Logger::Warning, tag, QString::asprintf(__VA_ARGS__))
#define LOG_ERROR(tag, ...) Logger::instance().log(Logger::Error,   tag, QString::asprintf(__VA_ARGS__))

// String-based overloads (no format string) / 字符串重载（无格式串）
#define LOG_DEBUG_S(tag, msg) Logger::instance().log(Logger::Debug,   tag, msg)
#define LOG_INFO_S(tag, msg)  Logger::instance().log(Logger::Info,    tag, msg)
#define LOG_WARN_S(tag, msg)  Logger::instance().log(Logger::Warning, tag, msg)
#define LOG_ERROR_S(tag, msg) Logger::instance().log(Logger::Error,   tag, msg)

#endif // LOGGER_H
