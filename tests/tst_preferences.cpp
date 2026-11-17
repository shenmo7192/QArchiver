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

private:
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

void TestPreferences::cleanupTestCase()
{
    QFile::remove(m_settingsFile);
    QDir().rmdir(QFileInfo(m_settingsFile).absolutePath());
}

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

QTEST_MAIN(TestPreferences)

#include "tst_preferences.moc"
