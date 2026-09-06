#include "CompressDialog.h"

#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGroupBox>

#include "core/AppSettings.h"

CompressDialog::CompressDialog(const QStringList &sourceFiles, QWidget *parent)
    : QDialog(parent)
    , m_sourceFiles(sourceFiles)
{
    setWindowTitle(tr("Create Archive"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QFileInfo fi(sourceFiles.first());
    QString defaultName = fi.completeBaseName();

    // Target
    auto *targetLabel = new QLabel(tr("Archive name:"), this);
    m_targetEdit = new QLineEdit(defaultName, this);
    auto *browseBtn = new QPushButton(tr("Browse..."), this);
    auto *targetLayout = new QHBoxLayout;
    targetLayout->addWidget(m_targetEdit);
    targetLayout->addWidget(browseBtn);

    // Format
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("7z (.7z)"), static_cast<int>(ArchiveFormat::SevenZip));
    m_formatCombo->addItem(tr("Zip (.zip)"), static_cast<int>(ArchiveFormat::Zip));
    m_formatCombo->addItem(tr("Tar (.tar)"), static_cast<int>(ArchiveFormat::Tar));
    m_formatCombo->addItem(tr("Gzip tar (.tar.gz)"), static_cast<int>(ArchiveFormat::TarGz));
    m_formatCombo->addItem(tr("Xz tar (.tar.xz)"), static_cast<int>(ArchiveFormat::TarXz));
    m_formatCombo->addItem(tr("Bzip2 tar (.tar.bz2)"), static_cast<int>(ArchiveFormat::TarBz2));

    // Compression level
    m_levelSpin = new QSpinBox(this);
    m_levelSpin->setRange(0, 9);
    m_levelSpin->setValue(AppSettings::instance().defaultCompressionLevel());

    // Password
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("(no encryption)"));
    m_encryptHeadersCheck = new QCheckBox(tr("Encrypt file names (7z only)"), this);

    // Volume
    m_volumeSpin = new QSpinBox(this);
    m_volumeSpin->setRange(0, 999999);
    m_volumeSpin->setValue(0);
    m_volumeUnit = new QComboBox(this);
    m_volumeUnit->addItem(tr("MB"), 1024 * 1024);
    m_volumeUnit->addItem(tr("KB"), 1024);

    // Symlinks
    m_followSymlinksCheck = new QCheckBox(tr("Follow symlinks"), this);
    m_followSymlinksCheck->setChecked(AppSettings::instance().followSymlinks());

    auto *form = new QFormLayout;
    form->addRow(tr("&Format:"), m_formatCombo);
    form->addRow(tr("Compression &level:"), m_levelSpin);
    form->addRow(tr("&Password:"), m_passwordEdit);
    form->addRow(QString(), m_encryptHeadersCheck);

    auto *volLayout = new QHBoxLayout;
    volLayout->addWidget(m_volumeSpin);
    volLayout->addWidget(m_volumeUnit);
    form->addRow(tr("&Volume size (0=no split):"), volLayout);
    form->addRow(QString(), m_followSymlinksCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browseBtn, &QPushButton::clicked, this, &CompressDialog::browseTarget);
    connect(m_formatCombo, &QComboBox::currentIndexChanged, this, &CompressDialog::onFormatChanged);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(targetLabel);
    layout->addLayout(targetLayout);
    layout->addLayout(form);
    layout->addWidget(buttons);

    onFormatChanged();
}

void CompressDialog::browseTarget()
{
    ArchiveFormat fmt = selectedFormat();
    QString ext = defaultExtension(fmt);
    QString suggested = m_targetEdit->text();
    if (!suggested.endsWith(ext, Qt::CaseInsensitive)) {
        suggested += ext;
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Select Archive Name"),
        suggested, tr("Archives (*%1)").arg(ext));
    if (!path.isEmpty()) {
        m_targetEdit->setText(path);
    }
}

void CompressDialog::onFormatChanged()
{
    ArchiveFormat fmt = selectedFormat();
    QString ext = defaultExtension(fmt);

    // Enable/disable encryption options
    bool supportsEnc = (fmt == ArchiveFormat::Zip || fmt == ArchiveFormat::SevenZip);
    m_passwordEdit->setEnabled(supportsEnc);
    m_encryptHeadersCheck->setEnabled(fmt == ArchiveFormat::SevenZip);

    // Update target extension if needed
    QString current = m_targetEdit->text();
    // Remove existing known archive extension
    for (const QString &e : {QStringLiteral(".7z"), QStringLiteral(".zip"),
                            QStringLiteral(".tar.gz"), QStringLiteral(".tar.xz"),
                            QStringLiteral(".tar.bz2"), QStringLiteral(".tar")}) {
        if (current.endsWith(e, Qt::CaseInsensitive)) {
            current.chop(e.size());
            break;
        }
    }
    m_targetEdit->setText(current + ext);
}

ArchiveFormat CompressDialog::selectedFormat() const
{
    return static_cast<ArchiveFormat>(m_formatCombo->currentData().toInt());
}

CompressOptions CompressDialog::options() const
{
    CompressOptions opts;
    opts.archivePath = m_targetEdit->text();
    opts.sourceFiles = m_sourceFiles;
    opts.compressionLevel = m_levelSpin->value();
    opts.password = m_passwordEdit->text();
    opts.encryptHeaders = m_encryptHeadersCheck->isChecked();
    if (m_volumeSpin->value() > 0) {
        opts.volumeSize = qint64(m_volumeSpin->value()) * m_volumeUnit->currentData().toLongLong();
    }
    opts.followSymlinks = m_followSymlinksCheck->isChecked();
    return opts;
}
