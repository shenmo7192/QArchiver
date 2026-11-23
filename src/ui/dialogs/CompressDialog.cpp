// QArchiver / 小Q解压
// CompressDialog widgets, validation and option collection.
// CompressDialog 的控件、校验与选项收集。

#include "common/FormatRegistry.h"
#include "ui/dialogs/CompressDialog.h"

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
#include <QSize>
#include <QFileInfo>
#include <QDir>
#include <QMessageBox>

#include "backends/rar/RarBackend.h"
#include "core/AppSettings.h"
#include "ui/icons/IconUtils.h"

CompressDialog::CompressDialog(const QStringList &sourceFiles, QWidget *parent)
    : QDialog(parent)
    , m_sourceFiles(sourceFiles)
{
    setWindowTitle(tr("Create Archive"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setMinimumWidth(520);

    const QFileInfo fi(sourceFiles.isEmpty() ? QString() : sourceFiles.first());
    const QString defaultName = fi.completeBaseName().isEmpty()
        ? QStringLiteral("archive")
        : fi.completeBaseName();
    const QString defaultPath = QDir(fi.absolutePath()).filePath(defaultName);

    // Target
    auto *targetLabel = new QLabel(tr("Archive name:"), this);
    m_targetEdit = new QLineEdit(defaultPath, this);
    auto *browseBtn = new QPushButton(Icons::aero(QStringLiteral("folder.png")),
                                      tr("Browse..."), this);
    auto *targetLayout = new QHBoxLayout;
    targetLayout->setSpacing(6);
    targetLayout->addWidget(m_targetEdit);
    targetLayout->addWidget(browseBtn);

    // Format
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(tr("7z (.7z)"), static_cast<int>(ArchiveFormat::SevenZip));
    m_formatCombo->addItem(tr("Zip (.zip)"), static_cast<int>(ArchiveFormat::Zip));
    // RAR creation needs the system rar command; hide it when unavailable.
    // RAR 创建依赖系统 rar 命令；未安装时不显示该选项。
    if (RarBackend::isCreateAvailable()) {
        m_formatCombo->addItem(tr("RAR (.rar)"), static_cast<int>(ArchiveFormat::Rar5));
    }
    m_formatCombo->addItem(tr("Tar (.tar)"), static_cast<int>(ArchiveFormat::Tar));
    m_formatCombo->addItem(tr("Gzip tar (.tar.gz)"), static_cast<int>(ArchiveFormat::TarGz));
    m_formatCombo->addItem(tr("Xz tar (.tar.xz)"), static_cast<int>(ArchiveFormat::TarXz));
    m_formatCombo->addItem(tr("Bzip2 tar (.tar.bz2)"), static_cast<int>(ArchiveFormat::TarBz2));
    m_formatCombo->addItem(tr("Gzip stream (.gz)"), static_cast<int>(ArchiveFormat::Gzip));
    m_formatCombo->addItem(tr("Bzip2 stream (.bz2)"), static_cast<int>(ArchiveFormat::Bzip2));
    m_formatCombo->addItem(tr("Xz stream (.xz)"), static_cast<int>(ArchiveFormat::Xz));
    const int formatIndex = m_formatCombo->findData(
        static_cast<int>(AppSettings::instance().defaultCompressFormat()));
    m_formatCombo->setCurrentIndex(formatIndex >= 0 ? formatIndex : 0);

    // Compression level
    m_levelSpin = new QSpinBox(this);
    m_levelSpin->setRange(0, 9);
    m_levelSpin->setValue(AppSettings::instance().defaultCompressionLevel());

    // Password
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText(tr("(no encryption)"));
    m_encryptHeadersCheck = new QCheckBox(tr("Encrypt file names (7z/RAR)"), this);

    // Volume
    m_volumeSpin = new QSpinBox(this);
    m_volumeSpin->setRange(0, 999999);
    m_volumeSpin->setValue(0);
    m_volumeUnit = new QComboBox(this);
    m_volumeUnit->addItem(tr("MB"), 1024 * 1024);
    m_volumeUnit->addItem(tr("KB"), 1024);

    // Additional options
    m_followSymlinksCheck = new QCheckBox(tr("Follow symlinks"), this);
    m_followSymlinksCheck->setChecked(AppSettings::instance().followSymlinks());
    m_solidCheck = new QCheckBox(tr("Solid compression (7z/RAR)"), this);
    m_storeFullPathCheck = new QCheckBox(tr("Store full/absolute paths"), this);

    m_excludeEdit = new QLineEdit(this);
    m_excludeEdit->setPlaceholderText(tr("Patterns separated by ';' (e.g. *.tmp;build/*)"));

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);
    form->addRow(tr("&Format:"), m_formatCombo);
    form->addRow(tr("Compression &level:"), m_levelSpin);
    form->addRow(tr("&Password:"), m_passwordEdit);
    form->addRow(QString(), m_encryptHeadersCheck);

    auto *volLayout = new QHBoxLayout;
    volLayout->setSpacing(6);
    volLayout->addWidget(m_volumeSpin);
    volLayout->addWidget(m_volumeUnit);
    form->addRow(tr("&Volume size (0=no split):"), volLayout);
    form->addRow(tr("&Exclude patterns:"), m_excludeEdit);
    form->addRow(QString(), m_followSymlinksCheck);
    form->addRow(QString(), m_solidCheck);
    form->addRow(QString(), m_storeFullPathCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->setCenterButtons(false);
    if (auto *ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setIcon(Icons::aero(QStringLiteral("yes.ico")));
        ok->setText(tr("OK"));
        ok->setIconSize(QSize(16, 16));
        ok->setMinimumWidth(75);
    }
    if (auto *cancel = buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setIcon(Icons::aero(QStringLiteral("no.ico")));
        cancel->setText(tr("Cancel"));
        cancel->setIconSize(QSize(16, 16));
        cancel->setMinimumWidth(75);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &CompressDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browseBtn, &QPushButton::clicked, this, &CompressDialog::browseTarget);
    connect(m_formatCombo, &QComboBox::currentIndexChanged,
            this, &CompressDialog::onFormatChanged);
    connect(m_passwordEdit, &QLineEdit::textChanged,
            this, &CompressDialog::updateOptionAvailability);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);
    layout->addWidget(targetLabel);
    layout->addLayout(targetLayout);
    layout->addLayout(form);
    layout->addWidget(buttons);

    onFormatChanged();
}

void CompressDialog::browseTarget()
{
    const ArchiveFormat fmt = selectedFormat();
    const QString ext = defaultExtension(fmt);
    QString suggested = m_targetEdit->text();
    if (suggested.isEmpty()) {
        suggested = QDir::homePath() + QStringLiteral("/archive");
    }
    if (!suggested.endsWith(ext, Qt::CaseInsensitive)) {
        suggested += ext;
    }

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Select Archive Name"),
        QFileInfo(suggested).absoluteFilePath(),
        tr("Archives (*%1)").arg(ext));
    if (!path.isEmpty()) {
        m_targetEdit->setText(path);
    }
}

void CompressDialog::onFormatChanged()
{
    const ArchiveFormat fmt = selectedFormat();
    const QString ext = defaultExtension(fmt);

    // Strip any known archive extension and append the selected one.
    // 去掉已知压缩包扩展名，再追加当前选择的扩展名。
    const QString current = FormatRegistry::stripArchiveExtension(
        m_targetEdit->text());
    m_targetEdit->setText(current + ext);

    updateOptionAvailability();
}

void CompressDialog::updateOptionAvailability()
{
    const ArchiveFormat fmt = selectedFormat();

    const bool rarCreation = isRarFamily(fmt) && RarBackend::isCreateAvailable();
    const bool supportsEncryption =
        fmt == ArchiveFormat::Zip || fmt == ArchiveFormat::SevenZip || rarCreation;
    const bool supportsVolumes = supportsEncryption;

    m_passwordEdit->setEnabled(supportsEncryption);
    if (!supportsEncryption) {
        m_passwordEdit->clear();
    }

    const bool hasPassword = !m_passwordEdit->text().isEmpty();
    m_encryptHeadersCheck->setEnabled(
        (fmt == ArchiveFormat::SevenZip || rarCreation) && hasPassword);
    if (!m_encryptHeadersCheck->isEnabled()) {
        m_encryptHeadersCheck->setChecked(false);
    }

    m_volumeSpin->setEnabled(supportsVolumes);
    m_volumeUnit->setEnabled(supportsVolumes);
    if (!supportsVolumes) {
        m_volumeSpin->setValue(0);
    }

    m_solidCheck->setEnabled(fmt == ArchiveFormat::SevenZip || rarCreation);
    if (fmt != ArchiveFormat::SevenZip && !rarCreation) {
        m_solidCheck->setChecked(false);
    }

    const bool pureStream = fmt == ArchiveFormat::Gzip
        || fmt == ArchiveFormat::Bzip2 || fmt == ArchiveFormat::Xz;
    m_followSymlinksCheck->setEnabled(!pureStream);
    m_storeFullPathCheck->setEnabled(!pureStream);
    m_excludeEdit->setEnabled(!pureStream);
    m_formatCombo->setToolTip(
        pureStream
            ? tr("A .gz / .bz2 / .xz stream can contain exactly one regular file.")
            : QString());

    switch (fmt) {
    case ArchiveFormat::Tar:
        m_levelSpin->setEnabled(false);
        break;
    case ArchiveFormat::TarGz:
    case ArchiveFormat::TarBz2:
    case ArchiveFormat::Gzip:
    case ArchiveFormat::Bzip2:
    case ArchiveFormat::Xz:
        m_levelSpin->setEnabled(true);
        m_levelSpin->setRange(1, 9);
        if (m_levelSpin->value() == 0) {
            m_levelSpin->setValue(6);
        }
        break;
    default:
        m_levelSpin->setEnabled(true);
        m_levelSpin->setRange(0, 9);
        break;
    }
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

    const ArchiveFormat fmt = selectedFormat();
    opts.compressionLevel = m_levelSpin->isEnabled() ? m_levelSpin->value() : 0;

    if (m_passwordEdit->isEnabled()) {
        opts.password = m_passwordEdit->text();
    }
    opts.encryptHeaders = m_encryptHeadersCheck->isEnabled()
        && m_encryptHeadersCheck->isChecked();

    if (m_volumeSpin->isEnabled() && m_volumeSpin->value() > 0) {
        opts.volumeSize = qint64(m_volumeSpin->value())
            * m_volumeUnit->currentData().toLongLong();
    }

    opts.followSymlinks = m_followSymlinksCheck->isEnabled()
        && m_followSymlinksCheck->isChecked();
    opts.solidMode = m_solidCheck->isEnabled() && m_solidCheck->isChecked();
    opts.storeFullPath = m_storeFullPathCheck->isEnabled()
        && m_storeFullPathCheck->isChecked();

    if (m_excludeEdit->isEnabled()) {
        const QStringList patterns = m_excludeEdit->text().split(
            QLatin1Char(';'), Qt::SkipEmptyParts);
        for (const QString &pattern : patterns) {
            if (!pattern.trimmed().isEmpty()) {
                opts.excludePatterns << pattern.trimmed();
            }
        }
    }

    Q_UNUSED(fmt);
    return opts;
}

void CompressDialog::accept()
{
    const QString target = m_targetEdit->text().trimmed();
    if (target.isEmpty()) {
        QMessageBox::warning(this, tr("Create Archive"),
                             tr("Please specify an archive name."));
        return;
    }
    if (m_sourceFiles.isEmpty()) {
        QMessageBox::warning(this, tr("Create Archive"),
                             tr("No source files were selected."));
        return;
    }

    const ArchiveFormat format = selectedFormat();
    const bool pureStream = format == ArchiveFormat::Gzip
        || format == ArchiveFormat::Bzip2 || format == ArchiveFormat::Xz;
    if (pureStream
        && (m_sourceFiles.size() != 1
            || QFileInfo(m_sourceFiles.first()).isDir())) {
        QMessageBox::warning(
            this, tr("Create Archive"),
            tr("A .gz / .bz2 / .xz stream can contain exactly one regular "
               "file. Select a single file, or choose .tar.gz / .tar.bz2 / "
               ".tar.xz to compress multiple files."));
        return;
    }

    const QFileInfo info(target);
    if (info.exists()) {
        const auto answer = QMessageBox::question(
            this, tr("Overwrite Archive"),
            tr("The archive already exists:\n%1\n\nOverwrite it?")
                .arg(info.absoluteFilePath()),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    QDialog::accept();
}
