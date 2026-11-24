#ifndef BENCHMARKDIALOG_H
#define BENCHMARKDIALOG_H

// QArchiver / 小Q解压
// 7-Zip benchmark runner.
// 7-Zip 性能测试对话框。

#include <QDialog>
#include <QElapsedTimer>
#include <QProcess>

#include "utils/LogLineBuffer.h"

class QProcess;
class QPlainTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QTimer;

// Runs the bundled `7z b` benchmark without blocking the GUI and streams its
// output.  Available from Preferences → Tools and from the Tools menu.
// 在不阻塞界面的情况下运行内置 `7z b` 性能测试并流式显示输出。
// 可从偏好设置 → 工具以及工具菜单打开。
class BenchmarkDialog : public QDialog {
    Q_OBJECT
public:
    explicit BenchmarkDialog(QWidget *parent = nullptr);
    ~BenchmarkDialog() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void startBenchmark();
    void stopBenchmark();
    void readOutput();
    void processFinished(int exitCode, QProcess::ExitStatus status);
    void processError(QProcess::ProcessError error);
    void updateElapsed();

private:
    void setRunning(bool running);
    void appendChunk(LogLineBuffer &buffer, const QByteArray &chunk);
    void flushBuffer(LogLineBuffer &buffer);
    void updateSummary();

    QProcess *m_process = nullptr;
    QPlainTextEdit *m_output = nullptr;
    QComboBox *m_threads = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_copyButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QTimer *m_elapsedTimer = nullptr;
    QElapsedTimer m_elapsed;
    // Separate buffers: stdout and stderr are independent streams and would
    // splice half lines together in a single buffer.
    // 分开缓冲：stdout 与 stderr 是独立流，共用缓冲区会把半行拼在一起。
    LogLineBuffer m_stdoutBuffer;
    LogLineBuffer m_stderrBuffer;
    int m_lastLogSecond = -1;
    bool m_running = false;
    bool m_stopping = false;
};

#endif // BENCHMARKDIALOG_H
