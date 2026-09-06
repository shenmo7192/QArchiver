#include "PreferencesDialog.h"

#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QLabel>
#include <QApplication>

#include "core/AppSettings.h"
#include "utils/ThemeManager.h"

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto &settings = AppSettings::instance();

    // Default format
    m_defaultFormatCombo = new QComboBox(this);
    m_defaultFormatCombo->addItem(tr("Zip"), static_cast<int>(ArchiveFormat::Zip));
    m_defaultFormatCombo->addItem(tr("7z"), static_cast<int>(ArchiveFormat::SevenZip));
    m_defaultFormatCombo->addItem(tr("Tar"), static_cast<int>(ArchiveFormat::Tar));
    m_defaultFormatCombo->addItem(tr("Tar.gz"), static_cast<int>(ArchiveFormat::TarGz));
    m_defaultFormatCombo->setCurrentIndex(m_defaultFormatCombo->findData(
        static_cast<int>(settings.defaultCompressFormat())));

    // Default level
    m_defaultLevelSpin = new QSpinBox(this);
    m_defaultLevelSpin->setRange(0, 9);
    m_defaultLevelSpin->setValue(settings.defaultCompressionLevel());

    // Overwrite policy
    m_overwriteCombo = new QComboBox(this);
    m_overwriteCombo->addItem(tr("Ask"), static_cast<int>(OverwritePolicy::Ask));
    m_overwriteCombo->addItem(tr("Always overwrite"), static_cast<int>(OverwritePolicy::Always));
    m_overwriteCombo->addItem(tr("Never overwrite"), static_cast<int>(OverwritePolicy::Never));
    m_overwriteCombo->setCurrentIndex(m_overwriteCombo->findData(
        static_cast<int>(settings.overwritePolicy())));

    m_preservePathsCheck = new QCheckBox(tr("Preserve directory structure"), this);
    m_preservePathsCheck->setChecked(settings.preservePaths());

    m_openAfterCheck = new QCheckBox(tr("Open destination after extraction"), this);
    m_openAfterCheck->setChecked(settings.openDestAfterFinish());

    m_maxPreviewSpin = new QSpinBox(this);
    m_maxPreviewSpin->setRange(1, 500);
    m_maxPreviewSpin->setSuffix(tr(" MB"));
    m_maxPreviewSpin->setValue(static_cast<int>(settings.maxPreviewSize() / (1024 * 1024)));

    m_followSymlinksCheck = new QCheckBox(tr("Follow symlinks when compressing"), this);
    m_followSymlinksCheck->setChecked(settings.followSymlinks());

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem(tr("System"), QStringLiteral("system"));
    m_themeCombo->addItem(tr("Light"), QStringLiteral("light"));
    m_themeCombo->addItem(tr("Dark"), QStringLiteral("dark"));
    m_themeCombo->setCurrentIndex(m_themeCombo->findData(settings.theme()));

    auto *form = new QFormLayout;
    form->addRow(tr("Default compression &format:"), m_defaultFormatCombo);
    form->addRow(tr("Default compression &level:"), m_defaultLevelSpin);
    form->addRow(tr("&Overwrite policy:"), m_overwriteCombo);
    form->addRow(tr("Max preview &size:"), m_maxPreviewSpin);
    form->addRow(tr("&Theme:"), m_themeCombo);
    form->addRow(QString(), m_preservePathsCheck);
    form->addRow(QString(), m_openAfterCheck);
    form->addRow(QString(), m_followSymlinksCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &PreferencesDialog::saveSettings);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

void PreferencesDialog::saveSettings()
{
    auto &s = AppSettings::instance();
    s.setDefaultCompressFormat(static_cast<ArchiveFormat>(m_defaultFormatCombo->currentData().toInt()));
    s.setDefaultCompressionLevel(m_defaultLevelSpin->value());
    s.setOverwritePolicy(static_cast<OverwritePolicy>(m_overwriteCombo->currentData().toInt()));
    s.setPreservePaths(m_preservePathsCheck->isChecked());
    s.setOpenDestAfterFinish(m_openAfterCheck->isChecked());
    s.setMaxPreviewSize(qint64(m_maxPreviewSpin->value()) * 1024 * 1024);
    s.setFollowSymlinks(m_followSymlinksCheck->isChecked());
    s.setTheme(m_themeCombo->currentData().toString());

    // Apply the theme immediately so the change is visible without restarting.
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
        ThemeManager::applyTheme(*app);
    }
}
