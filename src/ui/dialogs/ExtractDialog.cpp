#include "ExtractDialog.h"

#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QDialogButtonBox>
#include <QFileInfo>

#include "core/AppSettings.h"

ExtractDialog::ExtractDialog(const QString &archivePath, QWidget *parent)
    : QDialog(parent)
    , m_archivePath(archivePath)
{
    setWindowTitle(tr("Extract Archive"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QFileInfo fi(archivePath);

    // Default destination: the directory containing the opened archive.
    auto *destLabel = new QLabel(tr("Destination:"), this);
    m_destEdit = new QLineEdit(fi.absolutePath(), this);
    auto *browseBtn = new QPushButton(tr("Browse..."), this);

    auto *destLayout = new QHBoxLayout;
    destLayout->addWidget(m_destEdit);
    destLayout->addWidget(browseBtn);

    m_preservePathsCheck = new QCheckBox(tr("Preserve directory structure"), this);
    m_preservePathsCheck->setChecked(AppSettings::instance().preservePaths());

    m_openAfterCheck = new QCheckBox(tr("Open destination folder after extraction"), this);
    m_openAfterCheck->setChecked(AppSettings::instance().openDestAfterFinish());

    m_overwriteCombo = new QComboBox(this);
    m_overwriteCombo->addItem(tr("Ask"), static_cast<int>(OverwritePolicy::Ask));
    m_overwriteCombo->addItem(tr("Overwrite"), static_cast<int>(OverwritePolicy::Always));
    m_overwriteCombo->addItem(tr("Skip existing"), static_cast<int>(OverwritePolicy::Never));
    m_overwriteCombo->addItem(tr("Auto-rename"), static_cast<int>(OverwritePolicy::Rename));
    m_overwriteCombo->setCurrentIndex(0);

    auto *form = new QFormLayout;
    form->addRow(tr("&Overwrite policy:"), m_overwriteCombo);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browseBtn, &QPushButton::clicked, this, &ExtractDialog::browseDestDir);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(destLabel);
    layout->addLayout(destLayout);
    layout->addWidget(m_preservePathsCheck);
    layout->addWidget(m_openAfterCheck);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void ExtractDialog::browseDestDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Destination"),
        m_destEdit->text());
    if (!dir.isEmpty()) {
        m_destEdit->setText(dir);
    }
}

ExtractOptions ExtractDialog::options() const
{
    ExtractOptions opts;
    opts.destDir = m_destEdit->text();
    opts.preservePaths = m_preservePathsCheck->isChecked();
    opts.openDestAfterFinish = m_openAfterCheck->isChecked();
    opts.overwrite = static_cast<OverwritePolicy>(m_overwriteCombo->currentData().toInt());
    return opts;
}
