#include "ProgressDialog.h"

#include "utils/IconUtils.h"

#include <QProgressBar>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Working..."));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(true);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);

    m_fileLabel = new QLabel(tr("Processing..."), this);
    m_fileLabel->setWordWrap(true);

    m_statsLabel = new QLabel(this);

    m_output = new QPlainTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setMaximumBlockCount(1000);

    m_cancelBtn = new QPushButton(qarchiverIcon(QStringLiteral("no.ico")),
                                  tr("Cancel"), this);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_progressBar);
    layout->addWidget(m_fileLabel);
    layout->addWidget(m_statsLabel);
    layout->addWidget(m_output);
    layout->addWidget(m_cancelBtn, 0, Qt::AlignRight);

    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        m_cancelBtn->setEnabled(false);
        m_cancelBtn->setText(tr("Cancelling..."));
        m_fileLabel->setText(tr("Cancelling operation..."));
        emit cancelled();
    });
}

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

void ProgressDialog::appendOutput(const QString &line)
{
    m_output->appendPlainText(line);
    m_fileLabel->setText(line);
}

void ProgressDialog::setIndeterminate(bool indeterminate)
{
    if (indeterminate) {
        m_progressBar->setRange(0, 0);
    } else {
        m_progressBar->setRange(0, 100);
    }
}
