// QArchiver / 小Q解压
// BenchmarkDialog: QProcess wiring, live output and result summary.
// BenchmarkDialog：QProcess 连接、实时输出与结果摘要。

#include "ui/dialogs/BenchmarkDialog.h"

#include "utils/PlatformProcess.h"
#include "utils/SevenZipTool.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

BenchmarkDialog::BenchmarkDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("7-Zip Benchmark"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumSize(720, 520);

    auto *title = new QLabel(tr("7-Zip performance benchmark"), this);
    title->setObjectName(QStringLiteral("ModernBrandTitle"));
    auto *intro = new QLabel(
        tr("Measures the compression and decompression speed of the bundled "
           "7-Zip engine. The test can take about a minute. Output is streamed "
           "below and can be copied to the clipboard."),
        this);
    intro->setWordWrap(true);
    intro->setObjectName(QStringLiteral("WelcomeSubtitle"));

    m_threads = new QComboBox(this);
    m_threads->addItem(tr("Automatic"), 0);
    for (const int count : {1, 2, 4, 8, 12, 16, 32, 64}) {
        m_threads->addItem(tr("%1 threads").arg(count), count);
    }
    m_threads->setCurrentIndex(0);
    m_threads->setMinimumWidth(150);
    m_threads->setToolTip(
        tr("Number of threads used by the benchmark (Automatic = let 7-Zip choose)."));

    auto *options = new QHBoxLayout;
    options->setSpacing(8);
    auto *threadsLabel = new QLabel(tr("Threads:"), this);
    options->addWidget(threadsLabel);
    options->addWidget(m_threads);
    options->addStretch(1);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName(QStringLiteral("BenchmarkSummary"));
    m_summaryLabel->setTextFormat(Qt::PlainText);
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setVisible(false);

    m_output = new QPlainTextEdit(this);
    m_output->setObjectName(QStringLiteral("ProgressOutput"));
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(5000);
    QFont outputFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    outputFont.setPointSizeF(8.5);
    m_output->setFont(outputFont);
    m_output->setPlaceholderText(
        tr("Press Start to run the 7-Zip benchmark."));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("ModernDetailsValue"));
    m_statusLabel->setText(
        SevenZipTool::resolvePath().isEmpty()
            ? tr("The 7-Zip command-line tool was not found.")
            : tr("Ready."));

    m_startButton = new QPushButton(tr("Start"), this);
    m_startButton->setDefault(true);
    m_stopButton = new QPushButton(tr("Stop"), this);
    m_stopButton->setEnabled(false);
    m_copyButton = new QPushButton(tr("Copy Results"), this);
    m_copyButton->setEnabled(false);
    m_closeButton = new QPushButton(tr("Close"), this);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->setSpacing(8);
    buttonRow->addWidget(m_statusLabel, 1);
    buttonRow->addWidget(m_startButton);
    buttonRow->addWidget(m_stopButton);
    buttonRow->addWidget(m_copyButton);
    buttonRow->addWidget(m_closeButton);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);
    layout->addWidget(title);
    layout->addWidget(intro);
    layout->addLayout(options);
    layout->addWidget(m_summaryLabel);
    layout->addWidget(m_output, 1);
    layout->addLayout(buttonRow);

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    m_elapsedTimer = new QTimer(this);
    m_elapsedTimer->setInterval(1000);

    // Do not flash a console window when the GUI launches 7z.exe.
    // GUI 启动 7z.exe 时不要闪出控制台窗口。
    qarchiverHideConsoleWindow(*m_process);

    connect(m_startButton, &QPushButton::clicked,
            this, &BenchmarkDialog::startBenchmark);
    connect(m_stopButton, &QPushButton::clicked,
            this, &BenchmarkDialog::stopBenchmark);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::close);
    connect(m_copyButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_output->toPlainText());
    });
    connect(m_process, &QProcess::readyReadStandardOutput,
            this, &BenchmarkDialog::readOutput);
    connect(m_process, &QProcess::readyReadStandardError,
            this, &BenchmarkDialog::readOutput);
    connect(m_process, &QProcess::finished,
            this, &BenchmarkDialog::processFinished);
    connect(m_process, &QProcess::errorOccurred,
            this, &BenchmarkDialog::processError);
    connect(m_elapsedTimer, &QTimer::timeout,
            this, &BenchmarkDialog::updateElapsed);

    if (SevenZipTool::resolvePath().isEmpty()) {
        m_startButton->setEnabled(false);
    }
}

BenchmarkDialog::~BenchmarkDialog()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
}

void BenchmarkDialog::closeEvent(QCloseEvent *event)
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(1000);
    }
    QDialog::closeEvent(event);
}

void BenchmarkDialog::startBenchmark()
{
    if (m_running) {
        return;
    }
    const QString tool = SevenZipTool::resolvePath();
    if (tool.isEmpty()) {
        QMessageBox::warning(
            this, tr("7-Zip Benchmark"),
            tr("The 7-Zip command-line tool could not be found. Install it or "
               "place the bundled helper in the '7zip' directory next to the "
               "application."));
        return;
    }

    QStringList args;
    args << QStringLiteral("b");
    const int threadCount = m_threads->currentData().toInt();
    if (threadCount > 0) {
        args << QStringLiteral("-mmt=%1").arg(threadCount);
    }
    args << QStringLiteral("-sccUTF-8");

    m_output->clear();
    m_output->appendPlainText(
        tr("Running: %1 %2").arg(QFileInfo(tool).fileName(), args.join(QLatin1Char(' '))));
    m_output->appendPlainText(
        threadCount > 0
            ? tr("Threads: %1").arg(threadCount)
            : tr("Threads: Automatic"));
    m_output->appendPlainText(
        tr("7-Zip prints its result table when the benchmark finishes; "
           "this normally takes about a minute."));
    m_buffer.clear();
    m_lastLogSecond = -1;
    m_summaryLabel->clear();
    m_summaryLabel->setVisible(false);
    m_copyButton->setEnabled(false);
    m_statusLabel->setText(tr("Starting benchmark..."));
    m_elapsed.start();
    m_stopping = false;
    setRunning(true);
    m_process->start(tool, args);
    m_elapsedTimer->start();
}

void BenchmarkDialog::stopBenchmark()
{
    if (!m_running || !m_process) {
        return;
    }
    m_stopping = true;
    m_statusLabel->setText(tr("Stopping..."));
    m_stopButton->setEnabled(false);
    m_process->terminate();
    QTimer::singleShot(1500, this, [this]() {
        if (m_running && m_process
            && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    });
}

void BenchmarkDialog::readOutput()
{
    appendChunk(m_process->readAllStandardOutput());
    appendChunk(m_process->readAllStandardError());
}

void BenchmarkDialog::appendChunk(const QByteArray &chunk)
{
    if (chunk.isEmpty()) {
        return;
    }
    m_buffer += chunk;
    // Stream complete lines for a stable reading experience; keep a partial
    // line buffered until the next chunk arrives.
    int newline = -1;
    while ((newline = m_buffer.indexOf('\n')) >= 0) {
        QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        if (line.endsWith('\r')) {
            line.chop(1);
        }
        m_output->appendPlainText(QString::fromUtf8(line));
    }
}

void BenchmarkDialog::flushBuffer()
{
    if (m_buffer.isEmpty()) {
        return;
    }
    QByteArray line = m_buffer;
    m_buffer.clear();
    if (line.endsWith('\r')) {
        line.chop(1);
    }
    m_output->appendPlainText(QString::fromUtf8(line));
}

void BenchmarkDialog::processFinished(int exitCode, QProcess::ExitStatus status)
{
    readOutput();
    flushBuffer();
    m_elapsedTimer->stop();
    setRunning(false);

    const qint64 millis = m_elapsed.elapsed();
    const QString elapsed = tr("%1 s").arg(millis / 1000.0, 0, 'f', 1);
    if (m_stopping) {
        const QString stopped = tr("Benchmark stopped after %1.").arg(elapsed);
        m_statusLabel->setText(stopped);
        m_output->appendPlainText(stopped);
    } else if (status == QProcess::NormalExit && exitCode == 0) {
        const QString finished = tr("Benchmark finished in %1.").arg(elapsed);
        m_statusLabel->setText(finished);
        m_output->appendPlainText(finished);
    } else {
        const QString failed =
            tr("Benchmark failed (exit code %1) after %2.")
                .arg(exitCode).arg(elapsed);
        m_statusLabel->setText(failed);
        m_output->appendPlainText(failed);
    }
    m_stopping = false;
    m_copyButton->setEnabled(!m_output->toPlainText().isEmpty());
    updateSummary();
}

void BenchmarkDialog::processError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart) {
        m_elapsedTimer->stop();
        setRunning(false);
        m_statusLabel->setText(tr("Could not start the 7-Zip benchmark."));
        QMessageBox::critical(
            this, tr("7-Zip Benchmark"),
            tr("The 7-Zip benchmark process could not be started:\n%1")
                .arg(m_process->errorString()));
    }
}

void BenchmarkDialog::updateElapsed()
{
    if (!m_running) {
        return;
    }
    const qint64 seconds = m_elapsed.elapsed() / 1000;
    m_statusLabel->setText(
        tr("Benchmark running... %1 s").arg(seconds));
    // 7-Zip buffers benchmark stdout when it is redirected, so add a small
    // heartbeat to the log instead of leaving it blank until the process exits.
    // 7-Zip 在输出被重定向时会缓冲基准测试的 stdout，因此定期写入心跳，
    // 避免进程结束前日志一直是空的。
    if (seconds > 0 && seconds % 5 == 0 && seconds != m_lastLogSecond) {
        m_lastLogSecond = static_cast<int>(seconds);
        m_output->appendPlainText(
            tr("Benchmark running... %1 s").arg(seconds));
    }
}

void BenchmarkDialog::setRunning(bool running)
{
    m_running = running;
    m_startButton->setEnabled(!running
        && !SevenZipTool::resolvePath().isEmpty());
    m_stopButton->setEnabled(running);
    m_threads->setEnabled(!running);
}

void BenchmarkDialog::updateSummary()
{
    const QStringList lines = m_output->toPlainText().split(QLatin1Char('\n'));
    QStringList summary;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("Tot:"))
            || trimmed.contains(QLatin1String("MB/s"))
            || trimmed.startsWith(QLatin1String("Freq:"))) {
            summary << trimmed;
        }
    }
    while (summary.size() > 8) {
        summary.removeFirst();
    }
    if (summary.isEmpty()) {
        m_summaryLabel->clear();
        m_summaryLabel->setVisible(false);
        return;
    }
    m_summaryLabel->setText(summary.join(QLatin1Char('\n')));
    m_summaryLabel->setVisible(true);
}
