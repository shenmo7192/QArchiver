// QArchiver / 小Q解压
// ProgressDialog updates and cancellation forwarding.
// ProgressDialog 的更新与取消转发。

#include "ProgressDialog.h"

#include "utils/IconUtils.h"

#include <QProgressBar>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFont>
#include <QFontDatabase>
#include <QSize>

// Build the progress UI; cancellation is delegated through a signal.
// 构建进度界面；取消操作通过信号转发。
ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Working..."));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("ProgressBar"));
    m_progressBar->setRange(0, 100);

    m_fileLabel = new QLabel(tr("Processing..."), this);
    m_fileLabel->setWordWrap(true);

    m_statsLabel = new QLabel(this);

    m_output = new QPlainTextEdit(this);
    m_output->setObjectName(QStringLiteral("ProgressOutput"));
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(1000);
    QFont outputFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    outputFont.setPointSizeF(8.5);
    m_output->setFont(outputFont);

    m_cancelBtn = new QPushButton(qarchiverIcon(QStringLiteral("no.ico")),
                                  tr("Cancel"), this);
    m_cancelBtn->setObjectName(QStringLiteral("ProgressCancelButton"));
    m_cancelBtn->setIconSize(QSize(16, 16));
    m_cancelBtn->setMinimumWidth(90);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_fileLabel);
    layout->addWidget(m_statsLabel);
    layout->addWidget(m_output);
    layout->addWidget(m_cancelBtn, 0, Qt::AlignRight);

    resize(560, 380);

    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        m_cancelBtn->setEnabled(false);
        m_cancelBtn->setText(tr("Cancelling..."));
        m_fileLabel->setText(tr("Cancelling operation..."));
        emit cancelled();
    });
}

// Reset the dialog for a new operation.
// 为新操作重置对话框状态。
void ProgressDialog::setOperation(const QString &title)
{
    setWindowTitle(title);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_fileLabel->setText(tr("Processing..."));
    m_statsLabel->clear();
    m_output->clear();
    m_cancelBtn->setEnabled(true);
    m_cancelBtn->setText(tr("Cancel"));
}

// Update the bar and the "current / total files" statistics.
// 更新进度条以及“当前/总文件数”统计。
void ProgressDialog::updateProgress(int percent, int total, int current, int etaSec)
{
    if (m_progressBar->maximum() == 0) {
        m_progressBar->setRange(0, 100);
    }
    m_progressBar->setValue(percent);

    QString stats = tr("%1 / %2 files").arg(current).arg(total);
    if (etaSec >= 0) {
        int mins = etaSec / 60;
        int secs = etaSec % 60;
        stats += tr(" - ETA: %1:%2").arg(mins, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    } else {
        stats += tr(" - ETA: unknown");
    }
    m_statsLabel->setText(stats);
}

// Append one backend output line and mirror it in the status label.
// 追加一行后端输出，并同步显示在状态标签中。
void ProgressDialog::appendOutput(const QString &line)
{
    m_output->appendPlainText(line);
    m_fileLabel->setText(line);
}

// Switch between determinate (0..100) and busy (0..0) modes.
// 在确定进度（0..100）与忙碌模式（0..0）之间切换。
void ProgressDialog::setIndeterminate(bool indeterminate)
{
    if (indeterminate) {
        m_progressBar->setRange(0, 0);
    } else {
        m_progressBar->setRange(0, 100);
    }
}
