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
#include <QSpinBox>
#include <QPlainTextEdit>
#include <QSettings>

#include "core/AppSettings.h"
#include "core/ArchiveManager.h"
#include "core/RecentFilesManager.h"
#include "ui/FileListModel.h"
#include "ui/dialogs/PasswordDialog.h"
#include "ui/dialogs/PreferencesDialog.h"
#include "ui/dialogs/BenchmarkDialog.h"

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
    void configurableLimitsAreClamped();
    void recentFilesLimitTrimsImmediately();
    void newUiSettingsRoundTrip();
    void benchmarkDialogBuilds();
    void listModeDirectoryIndexPreservesEntries();

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

// New configurable limits have stable defaults and strict ranges.
// 新增可配置上限具有稳定默认值并严格限制范围。
void TestPreferences::configurableLimitsAreClamped()
{
    AppSettings &settings = AppSettings::instance();

    settings.setContinuousImageDialogThreshold(-1);
    QCOMPARE(settings.continuousImageDialogThreshold(), 0);
    settings.setContinuousImageDialogThreshold(10000);
    QCOMPARE(settings.continuousImageDialogThreshold(), 9999);
    settings.setContinuousImageDialogThreshold(150);
    QCOMPARE(settings.continuousImageDialogThreshold(), 150);

    settings.setRecentFilesLimit(0);
    QCOMPARE(settings.recentFilesLimit(), 1);
    settings.setRecentFilesLimit(101);
    QCOMPARE(settings.recentFilesLimit(), 100);
    settings.setRecentFilesLimit(7);
    QCOMPARE(settings.recentFilesLimit(), 7);
}

// Reducing the recent-files limit trims the stored history without waiting
// for the next "add", and the sidebar notification is emitted.
// 降低最近文件上限时立即裁剪已保存历史，无需等待下一次“添加”，
// 并发出侧边栏通知。
void TestPreferences::recentFilesLimitTrimsImmediately()
{
    AppSettings &settings = AppSettings::instance();
    RecentFilesManager &recent = RecentFilesManager::instance();

    settings.setRecentFilesLimit(3);
    settings.setRecentFiles({QStringLiteral("a"), QStringLiteral("b"),
                             QStringLiteral("c"), QStringLiteral("d"),
                             QStringLiteral("e")});
    QCOMPARE(settings.recentFiles().size(), 3);

    QSignalSpy changedSpy(&recent, &RecentFilesManager::changed);
    settings.setRecentFilesLimit(1);
    QCOMPARE(settings.recentFiles().size(), 1);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(recent.list().size(), 1);
}

// Modern/classic layout and one-click settings have stable values.
// 现代/经典布局与一键解压设置具有稳定的取值。
void TestPreferences::newUiSettingsRoundTrip()
{
    AppSettings &settings = AppSettings::instance();
    const QString savedLayout = settings.uiLayoutMode();
    const QString savedDestMode = settings.oneClickExtractDestMode();
    const QString savedDest = settings.oneClickExtractDest();
    const bool savedClose = settings.oneClickCloseAfterExtract();
    const bool savedAsked = settings.oneClickCloseAsked();
    const QString savedViewMode = settings.contentViewMode();

    settings.setUiLayoutMode(QStringLiteral("classic"));
    QCOMPARE(settings.uiLayoutMode(), QStringLiteral("classic"));
    settings.setUiLayoutMode(QStringLiteral("modern"));
    QCOMPARE(settings.uiLayoutMode(), QStringLiteral("modern"));
    settings.setUiLayoutMode(QStringLiteral("unsupported"));
    QCOMPARE(settings.uiLayoutMode(), QStringLiteral("modern"));

    settings.setOneClickExtractDestMode(QStringLiteral("custom"));
    settings.setOneClickExtractDest(QStringLiteral("/tmp/one-click"));
    settings.setOneClickCloseAfterExtract(true);
    settings.setOneClickCloseAsked(true);
    QCOMPARE(settings.oneClickExtractDestMode(), QStringLiteral("custom"));
    QCOMPARE(settings.oneClickExtractDest(), QStringLiteral("/tmp/one-click"));
    QVERIFY(settings.oneClickCloseAfterExtract());
    QVERIFY(settings.oneClickCloseAsked());

    settings.setContentViewMode(QStringLiteral("grid"));
    QCOMPARE(settings.contentViewMode(), QStringLiteral("grid"));
    settings.setContentViewMode(QStringLiteral("list"));
    QCOMPARE(settings.contentViewMode(), QStringLiteral("list"));
    settings.setContentViewMode(QStringLiteral("tree"));
    QCOMPARE(settings.contentViewMode(), QStringLiteral("tree"));
    settings.setContentViewMode(QStringLiteral("unsupported"));
    QCOMPARE(settings.contentViewMode(), QStringLiteral("grid"));

    settings.setOneClickExtractDestMode(QStringLiteral("archive"));
    settings.setOneClickExtractDest(savedDest);
    settings.setOneClickCloseAfterExtract(savedClose);
    settings.setOneClickCloseAsked(savedAsked);
    settings.setUiLayoutMode(savedLayout);
    settings.setContentViewMode(savedViewMode);
}

// The benchmark dialog can be created without a display and exposes its
// thread-count control.
// 性能测试对话框可在无显示环境下创建，并包含线程数控件。
void TestPreferences::benchmarkDialogBuilds()
{
    BenchmarkDialog dialog;
    QVERIFY(dialog.findChild<QSpinBox *>() != nullptr);
    QVERIFY(dialog.findChild<QPlainTextEdit *>() != nullptr);
}

// List mode uses the directory index: implicit parents, duplicates and the
// original member spelling survive navigation.
// 列表模式使用目录索引：隐式父目录、重复项和原始成员名在导航后仍保留。
void TestPreferences::listModeDirectoryIndexPreservesEntries()
{
    FileListModel model;
    QList<ArchiveEntry> entries;
    auto add = [&entries](const QString &path, qint64 size = 0) {
        ArchiveEntry entry;
        entry.path = path;
        entry.size = size;
        entries.append(entry);
    };
    add(QStringLiteral("./root.txt"), 1);
    add(QStringLiteral("dir/a.txt"), 2);
    add(QStringLiteral("dir/a.txt"), 3);
    add(QStringLiteral("dir/sub/b.txt"), 4);

    model.setEntries(entries);
    model.setFlatMode(true);

    QCOMPARE(model.rowCount(), 2); // implicit "dir" + root.txt
    QVERIFY(model.enterDirectory(QStringLiteral("dir")));
    QCOMPARE(model.rowCount(), 4); // ".." + two duplicate a.txt rows + sub
    QVERIFY(model.enterDirectory(QStringLiteral("sub")));
    QCOMPARE(model.rowCount(), 2); // ".." + b.txt
    QCOMPARE(model.index(1, FileListModel::NameColumn)
                 .data(FileListModel::PathRole).toString(),
             QStringLiteral("dir/sub/b.txt"));
    QVERIFY(model.goUp());
    QVERIFY(model.goUp());
    QCOMPARE(model.rowCount(), 2);
}

QTEST_MAIN(TestPreferences)

#include "tst_preferences.moc"
