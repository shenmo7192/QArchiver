// QArchiver / 小Q解压
// Tests for preferences and password dialog wiring.
// 偏好设置与密码对话框连接测试。

#include <QtTest>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPushButton>
#include <QSettings>

#include "core/AppSettings.h"
#include "core/ArchiveManager.h"
#include "ui/dialogs/PasswordDialog.h"
#include "ui/dialogs/PreferencesDialog.h"

// The preferences dialog is the only place where the libarchive switch is
// exposed, so the wiring between the checkbox and AppSettings is tested here.
class TestPreferences : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void libArchiveCheckboxReflectsSetting();
    void libArchiveCheckboxPersistsChoice();
    void libArchiveCheckboxDisabledWithoutLibrary();
    void passwordDialogUsesRememberSetting();

private:
    static QCheckBox *passwordRememberCheck(PasswordDialog &dialog)
    {
        const auto boxes = dialog.findChildren<QCheckBox *>();
        return boxes.size() == 1 ? boxes.first() : nullptr;
    }

    static QCheckBox *libArchiveCheck(PreferencesDialog &dialog)
    {
        const auto boxes = dialog.findChildren<QCheckBox *>();
        for (QCheckBox *box : boxes) {
            if (box->text().contains(QStringLiteral("libarchive"),
                                     Qt::CaseInsensitive)) {
                return box;
            }
        }
        return nullptr;
    }

    static void acceptDialog(PreferencesDialog &dialog)
    {
        auto *buttons = dialog.findChild<QDialogButtonBox *>();
        QVERIFY(buttons);
        QPushButton *ok = buttons->button(QDialogButtonBox::Ok);
        QVERIFY(ok);
        ok->click();
    }

    QString m_settingsFile;
};

// Create an isolated settings environment.
// 创建隔离的设置环境。
void TestPreferences::initTestCase()
{
    // Keep the developer's real configuration untouched.
    QCoreApplication::setOrganizationName(QStringLiteral("QArchiverTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_preferences"));
    m_settingsFile = QSettings(QSettings::IniFormat, QSettings::UserScope,
                               QStringLiteral("QArchiverTests"),
                               QStringLiteral("tst_preferences"))
                         .fileName();
    QFile::remove(m_settingsFile);
}

// Restore the previous settings environment.
// 恢复之前的设置环境。
void TestPreferences::cleanupTestCase()
{
    QFile::remove(m_settingsFile);
    QDir().rmdir(QFileInfo(m_settingsFile).absolutePath());
}

// The checkbox reflects the persisted preference.
// 复选框反映已持久化的偏好设置。
void TestPreferences::libArchiveCheckboxReflectsSetting()
{
    if (!ArchiveManager::isLibArchiveAvailable()) {
        QSKIP("built without libarchive: the switch is always off and disabled");
    }

    AppSettings &settings = AppSettings::instance();
    const bool saved = settings.useLibArchiveBackend();

    settings.setUseLibArchiveBackend(false);
    {
        PreferencesDialog dialog;
        QCheckBox *box = libArchiveCheck(dialog);
        QVERIFY2(box, "the libarchive checkbox is missing");
        QVERIFY(!box->isChecked());
    }

    settings.setUseLibArchiveBackend(true);
    {
        PreferencesDialog dialog;
        QCheckBox *box = libArchiveCheck(dialog);
        QVERIFY(box);
        QVERIFY(box->isChecked());
    }

    settings.setUseLibArchiveBackend(saved);
}

// Changing the checkbox persists the preference.
// 修改复选框会持久化偏好设置。
void TestPreferences::libArchiveCheckboxPersistsChoice()
{
    if (!ArchiveManager::isLibArchiveAvailable()) {
        QSKIP("built without libarchive: the switch is always off and disabled");
    }

    AppSettings &settings = AppSettings::instance();
    const bool saved = settings.useLibArchiveBackend();
    settings.setUseLibArchiveBackend(!saved);

    {
        PreferencesDialog dialog;
        QCheckBox *box = libArchiveCheck(dialog);
        QVERIFY(box);
        QCOMPARE(box->isChecked(), !saved);

        // Toggling the box and pressing OK must persist the new value.
        box->setChecked(saved);
        acceptDialog(dialog);
        QCOMPARE(settings.useLibArchiveBackend(), saved);
    }

    settings.setUseLibArchiveBackend(saved);
}

// The option is disabled when libarchive is unavailable.
// libarchive 不可用时该选项被禁用。
void TestPreferences::libArchiveCheckboxDisabledWithoutLibrary()
{
    PreferencesDialog dialog;
    QCheckBox *box = libArchiveCheck(dialog);
    QVERIFY(box);

    if (ArchiveManager::isLibArchiveAvailable()) {
        QVERIFY(box->isEnabled());
    } else {
        QVERIFY(!box->isEnabled());
        QVERIFY(!box->isChecked());
    }

    // The environment escape hatch also turns the switch off.
    qputenv("QARCHIVER_NO_LIBARCHIVE", "1");
    {
        PreferencesDialog envDialog;
        QCheckBox *envBox = libArchiveCheck(envDialog);
        QVERIFY(envBox);
        QVERIFY(!envBox->isEnabled());
        QVERIFY(!envBox->isChecked());
    }
    qunsetenv("QARCHIVER_NO_LIBARCHIVE");
}

// Password dialog defaults to and never rewrites the preference.
// 密码对话框以偏好设置为默认值，且绝不回写偏好设置。
void TestPreferences::passwordDialogUsesRememberSetting()
{
    AppSettings &settings = AppSettings::instance();
    const bool saved = settings.rememberPasswords();

    settings.setRememberPasswords(false);
    {
        PasswordDialog dialog;
        QCheckBox *box = passwordRememberCheck(dialog);
        QVERIFY(box);
        QVERIFY(!dialog.rememberPassword());
        QVERIFY(!box->isChecked());

        // Opting in for this prompt must not write the preference.
        box->setChecked(true);
        QVERIFY(dialog.rememberPassword());
        QVERIFY(!settings.rememberPasswords());
    }

    settings.setRememberPasswords(true);
    {
        PasswordDialog dialog;
        QCheckBox *box = passwordRememberCheck(dialog);
        QVERIFY(box);
        QVERIFY(dialog.rememberPassword());
        QVERIFY(box->isChecked());

        // Opting out for this prompt must not write the preference either.
        box->setChecked(false);
        QVERIFY(!dialog.rememberPassword());
        QVERIFY(settings.rememberPasswords());
    }

    settings.setRememberPasswords(saved);
}

QTEST_MAIN(TestPreferences)

#include "tst_preferences.moc"
