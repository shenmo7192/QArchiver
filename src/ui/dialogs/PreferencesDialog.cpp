// QArchiver / 小Q解压
// PreferencesDialog widget wiring and persistence.
// PreferencesDialog 的控件连接与持久化。

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
#include "core/ArchiveManager.h"
#include "utils/ThemeManager.h"
#include "utils/IconUtils.h"

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
    const int formatIndex = m_defaultFormatCombo->findData(
        static_cast<int>(settings.defaultCompressFormat()));
    m_defaultFormatCombo->setCurrentIndex(formatIndex >= 0 ? formatIndex : 0);

    // Default level
    m_defaultLevelSpin = new QSpinBox(this);
    m_defaultLevelSpin->setRange(0, 9);
    m_defaultLevelSpin->setValue(settings.defaultCompressionLevel());

    // Overwrite policy
    m_overwriteCombo = new QComboBox(this);
    m_overwriteCombo->addItem(tr("Ask"), static_cast<int>(OverwritePolicy::Ask));
    m_overwriteCombo->addItem(tr("Always overwrite"), static_cast<int>(OverwritePolicy::Always));
    m_overwriteCombo->addItem(tr("Never overwrite"), static_cast<int>(OverwritePolicy::Never));
    const int overwriteIndex = m_overwriteCombo->findData(
        static_cast<int>(settings.overwritePolicy()));
    m_overwriteCombo->setCurrentIndex(overwriteIndex >= 0 ? overwriteIndex : 0);

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

    m_rememberPasswordsCheck = new QCheckBox(tr("Remember passwords for the session"), this);
    m_rememberPasswordsCheck->setChecked(settings.rememberPasswords());
    m_rememberPasswordsCheck->setToolTip(
        tr("Passwords are kept in memory only and are never written to disk."));

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem(tr("System"), QStringLiteral("system"));
    m_themeCombo->addItem(tr("Light"), QStringLiteral("light"));
    m_themeCombo->addItem(tr("Dark"), QStringLiteral("dark"));
    const int themeIndex = m_themeCombo->findData(settings.theme());
    m_themeCombo->setCurrentIndex(themeIndex >= 0 ? themeIndex : 0);

    m_viewModeCombo = new QComboBox(this);
    m_viewModeCombo->addItem(tr("Tree view"), QStringLiteral("tree"));
    m_viewModeCombo->addItem(tr("List view"), QStringLiteral("list"));
    const int viewModeIndex = m_viewModeCombo->findData(settings.contentViewMode());
    m_viewModeCombo->setCurrentIndex(viewModeIndex >= 0 ? viewModeIndex : 0);

    m_toolbarStyleCombo = new QComboBox(this);
    m_toolbarStyleCombo->addItem(tr("Icons above text"),
                                 QStringLiteral("textUnderIcon"));
    m_toolbarStyleCombo->addItem(tr("Icons beside text"),
                                 QStringLiteral("textBesideIcon"));
    m_toolbarStyleCombo->addItem(tr("Text only"),
                                 QStringLiteral("textOnly"));
    m_toolbarStyleCombo->addItem(tr("Icons only (tooltips)"),
                                 QStringLiteral("iconOnly"));
    const int toolbarStyleIndex = m_toolbarStyleCombo->findData(settings.toolButtonStyle());
    m_toolbarStyleCombo->setCurrentIndex(toolbarStyleIndex >= 0 ? toolbarStyleIndex : 0);

    m_showRecentFilesCheck = new QCheckBox(tr("Show recent files sidebar"), this);
    m_showRecentFilesCheck->setChecked(settings.showRecentFiles());

    m_libArchiveCheck = new QCheckBox(
        tr("Read archives with libarchive (no external process)"), this);
    const bool libArchiveUsable = ArchiveManager::isLibArchiveAvailable()
        && !ArchiveManager::isLibArchiveDisabledByEnvironment();
    m_libArchiveCheck->setChecked(settings.useLibArchiveBackend()
                                  && libArchiveUsable);
    if (libArchiveUsable) {
        m_libArchiveCheck->setToolTip(
            tr("zip, tar, 7z and gz/bz2/xz archives are read inside the "
               "application. Archives that libarchive cannot open (encrypted "
               "7z, split volumes, rar) automatically switch to the command "
               "line tools. The change applies to the next archive you open."));
        m_libArchiveHint = new QLabel(
            tr("Applies to the next archive you open; the currently open "
               "archive keeps the backend it started with."), this);
        m_libArchiveHint->setWordWrap(true);
        m_libArchiveHint->setEnabled(false);
    } else {
        m_libArchiveCheck->setChecked(false);
        m_libArchiveCheck->setEnabled(false);
        m_libArchiveCheck->setToolTip(
            ArchiveManager::isLibArchiveAvailable()
                ? tr("Disabled by the QARCHIVER_NO_LIBARCHIVE environment variable.")
                : tr("This build was compiled without libarchive."));
    }

    auto *form = new QFormLayout;
    form->addRow(tr("Default compression &format:"), m_defaultFormatCombo);
    form->addRow(tr("Default compression &level:"), m_defaultLevelSpin);
    form->addRow(tr("&Overwrite policy:"), m_overwriteCombo);
    form->addRow(tr("Max preview &size:"), m_maxPreviewSpin);
    form->addRow(tr("&Theme:"), m_themeCombo);
    form->addRow(tr("Archive content &view:"), m_viewModeCombo);
    form->addRow(tr("Toolbar button style:"), m_toolbarStyleCombo);
    form->addRow(QString(), m_preservePathsCheck);
    form->addRow(QString(), m_openAfterCheck);
    form->addRow(QString(), m_followSymlinksCheck);
    form->addRow(QString(), m_rememberPasswordsCheck);
    form->addRow(QString(), m_libArchiveCheck);
    if (m_libArchiveHint) {
        form->addRow(QString(), m_libArchiveHint);
    }
    form->addRow(QString(), m_showRecentFilesCheck);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto *ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setIcon(qarchiverIcon(QStringLiteral("yes.ico")));
    }
    if (auto *cancel = buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setIcon(qarchiverIcon(QStringLiteral("no.ico")));
    }
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
    s.setRememberPasswords(m_rememberPasswordsCheck->isChecked());
    if (m_libArchiveCheck->isEnabled()) {
        s.setUseLibArchiveBackend(m_libArchiveCheck->isChecked());
    }
    s.setTheme(m_themeCombo->currentData().toString());
    s.setContentViewMode(m_viewModeCombo->currentData().toString());
    s.setToolButtonStyle(m_toolbarStyleCombo->currentData().toString());
    s.setShowRecentFiles(m_showRecentFilesCheck->isChecked());

    // Apply the theme immediately so the change is visible without restarting.
    // 立即应用主题，无需重启即可看到变化。
    if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance())) {
        ThemeManager::applyTheme(*app);
    }
}
