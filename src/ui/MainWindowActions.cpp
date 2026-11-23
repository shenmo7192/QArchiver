// QArchiver / 小Q解压
// MainWindow actions: QAction setup, menus, tool button style, state updates and
// the menu/toolbar command handlers.
// MainWindow 动作：QAction 构建、菜单、工具栏按钮样式、状态更新以及
// 菜单/工具栏命令处理。

// QArchiver / 小Q解压
// MainWindow implementation: modern and classic layouts, actions, drag/drop,
// previews, one-click extraction and dialogs.
// MainWindow 实现：现代与经典布局、动作、拖放、预览、一键解压与对话框。

#include "ui/MainWindow.h"

#include "ui/MainWindowHelpers.h"
#include "ui/icons/IconUtils.h"

#include "ui/models/FileListModel.h"
#include "ui/models/FilterProxyModel.h"
#include "ui/models/FileListViewDelegate.h"
#include "ui/services/PreviewManager.h"
#include "ui/services/PreviewUtils.h"
#include "ui/services/DragOutManager.h"
#include "ui/widgets/ArchiveTreeView.h"
#include "ui/widgets/ArchiveGridView.h"
#include "ui/widgets/BackgroundWidget.h"
#include "ui/widgets/SidebarWidget.h"
#include "ui/widgets/ModernToolBar.h"
#include "ui/widgets/ModernNavBar.h"
#include "ui/widgets/ArchiveFolderPane.h"
#include "ui/widgets/WelcomePage.h"
#include "ui/dialogs/ExtractDialog.h"
#include "ui/dialogs/CompressDialog.h"
#include "ui/dialogs/PasswordDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/PreferencesDialog.h"
#include "ui/dialogs/BenchmarkDialog.h"
#include "utils/Logger.h"
#include "utils/MimeTypeHelper.h"
#include "utils/SevenZipTool.h"
#include "ui/icons/MessageBoxUtils.h"
#include "utils/FileUtils.h"
#include "ui/icons/FileTypeHelper.h"
#include "ui/theme/ThemeManager.h"

#include "core/ArchiveManager.h"
#include "common/ArchiveError.h"
#include "common/ArchiveOptions.h"
#include "common/ArchiveConstants.h"
#include "core/AppSettings.h"

#include "backends/ArchiveBackend.h"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent>

#include <limits>

void MainWindow::openPreferences()
{
    onPreferences();
}

void MainWindow::openBenchmark()
{
    onBenchmark();
}

void MainWindow::checkSevenZipFallback()
{
    static bool started = false;
    if (started) {
        return;
    }
    started = true;

    // Probe the fallback CLI off the GUI thread so --help/codec detection
    // cannot delay the first paint.
    // 在 GUI 线程之外探测兜底 CLI，避免 --help/编解码器检测阻塞首帧。
    auto *watcher = new QFutureWatcher<SevenZipToolInfo>(this);
    connect(watcher, &QFutureWatcher<SevenZipToolInfo>::finished, this,
            [this, watcher]() {
                const SevenZipToolInfo info = watcher->result();
                watcher->deleteLater();
                showSevenZipFallbackInfo(info);
            });
    watcher->setFuture(QtConcurrent::run([]() { return SevenZipTool::detect(); }));
}

void MainWindow::showSevenZipFallbackInfo(const SevenZipToolInfo &info)
{
    if (info.source == SevenZipToolInfo::Source::Bundled
        || info.source == SevenZipToolInfo::Source::Override) {
        return;
    }

    if (info.source == SevenZipToolInfo::Source::System) {
        const QString rarStatus = info.supportsRar
            ? tr("RAR extraction: supported by this system 7-Zip.")
            : tr("RAR extraction: NOT supported by this system 7-Zip. "
                 "On Debian/Ubuntu install the '7zip-rar' package "
                 "(older releases: 'p7zip-rar') to enable it.");

        QStringList dirs = info.bundledDirectories;
        if (dirs.isEmpty()) {
            dirs << QDir(QCoreApplication::applicationDirPath())
                        .filePath(QStringLiteral("7zip"));
        }

        QMessageBox::warning(
            this, tr("System 7-Zip Fallback"),
            tr("The bundled 7-Zip command-line tool was not found in the "
               "program directories:\n%1\n\n"
               "QArchiver is using the system 7-Zip instead:\n%2\n\n"
               "%3\n\n"
               "For a self-contained installation, download the official "
               "7-Zip command-line tool and place it in the '7zip' directory "
               "next to QArchiver.")
                .arg(dirs.join(QLatin1Char('\n')), info.path, rarStatus));
        return;
    }

    QString targetDir = info.bundledDirectories.value(0);
    if (targetDir.isEmpty()) {
        targetDir = QDir(QCoreApplication::applicationDirPath())
                        .filePath(QStringLiteral("7zip"));
    }

#ifdef Q_OS_WIN
    const QString hint = tr(
        "Download the official 7-Zip command-line tool from "
        "https://www.7-zip.org/download.html and place 7z.exe together with "
        "the matching 7z.dll in:\n%1\n\nThen restart QArchiver.").arg(targetDir);
#else
    const QString hint = tr(
        "Fix this in one of these ways:\n\n"
        "1. Download the official 7-Zip command-line tool from "
        "https://www.7-zip.org/download.html and place the executable in:\n"
        "%1\n\n"
        "2. On Debian/Ubuntu, install the system packages:\n"
        "   sudo apt update\n"
        "   sudo apt install 7zip\n"
        "   sudo apt install 7zip-rar\n"
        "   (older releases: sudo apt install p7zip-full p7zip-rar)\n\n"
        "Then restart QArchiver.").arg(targetDir);
#endif
    QMessageBox::critical(
        this, tr("No 7-Zip Fallback"),
        tr("No usable 7-Zip command-line tool was found in the program "
           "directories or in PATH.\n\n"
           "QArchiver uses 7-Zip as the fallback backend for archive formats "
           "and operations the built-in backend cannot handle. Without it, "
           "some archives cannot be opened, extracted or created.\n\n%1")
            .arg(hint));
}

void MainWindow::setupActions()
{
    m_openAct = new QAction(tr("Open"), this);
    m_openAct->setShortcut(QKeySequence::Open);
    connect(m_openAct, &QAction::triggered, this, &MainWindow::onOpen);

    m_newAct = new QAction(tr("Compress"), this);
    m_newAct->setShortcut(QKeySequence::New);
    m_newAct->setToolTip(tr("Create a new archive (Ctrl+N)"));
    connect(m_newAct, &QAction::triggered, this, &MainWindow::onNewArchive);

    m_extractAct = new QAction(tr("Extract To..."), this);
    m_extractAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(m_extractAct, &QAction::triggered, this, &MainWindow::onExtractAll);

    m_oneClickAct = new QAction(tr("One-click Extract"), this);
    m_oneClickAct->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
    connect(m_oneClickAct, &QAction::triggered,
            this, &MainWindow::onOneClickExtract);

    m_testAct = new QAction(tr("Test"), this);
    m_testAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(m_testAct, &QAction::triggered, this, &MainWindow::onTestArchive);

    m_addAct = new QAction(tr("Add"), this);
    m_addAct->setToolTip(tr("Add files to the current archive"));
    connect(m_addAct, &QAction::triggered, this, &MainWindow::onAddFiles);

    m_deleteAct = new QAction(tr("Delete"), this);
    m_deleteAct->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Delete));
    connect(m_deleteAct, &QAction::triggered,
            this, &MainWindow::onDeleteSelected);

    m_closeAct = new QAction(tr("Close"), this);
    m_closeAct->setShortcut(QKeySequence::Close);
    m_closeAct->setToolTip(tr("Close the current archive"));
    connect(m_closeAct, &QAction::triggered, this, &MainWindow::onCloseArchive);

    m_prefsAct = new QAction(tr("Preferences"), this);
    connect(m_prefsAct, &QAction::triggered, this, &MainWindow::onPreferences);

    m_aboutAct = new QAction(tr("About"), this);
    connect(m_aboutAct, &QAction::triggered, this, &MainWindow::onAbout);

    m_quitAct = new QAction(tr("Quit"), this);
    m_quitAct->setShortcut(QKeySequence::Quit);
    connect(m_quitAct, &QAction::triggered, qApp, &QApplication::quit);

    applyActionIcons();
}

void MainWindow::applyActionIcons()
{
    const auto classic = [](const QString &name) {
        return Icons::aero(name);
    };
    const auto modern = [](const QString &name) {
        return Icons::modern(name);
    };

    if (m_modernUi) {
        m_openAct->setIcon(modern(QStringLiteral("open.png")));
        m_newAct->setIcon(modern(QStringLiteral("compress.png")));
        m_extractAct->setIcon(modern(QStringLiteral("extract.png")));
        m_oneClickAct->setIcon(modern(QStringLiteral("oneclick.png")));
        m_testAct->setIcon(modern(QStringLiteral("test.png")));
        m_addAct->setIcon(modern(QStringLiteral("add.png")));
        m_deleteAct->setIcon(modern(QStringLiteral("delete.png")));
        m_closeAct->setIcon(modern(QStringLiteral("close.png")));
        m_prefsAct->setIcon(modern(QStringLiteral("settings.png")));
        m_aboutAct->setIcon(modern(QStringLiteral("help.png")));
        m_quitAct->setIcon(modern(QStringLiteral("close.png")));
    } else {
        m_openAct->setIcon(classic(QStringLiteral("Open.png")));
        m_newAct->setIcon(classic(QStringLiteral("New.png")));
        m_extractAct->setIcon(classic(QStringLiteral("Extract.png")));
        m_oneClickAct->setIcon(classic(QStringLiteral("Extract.png")));
        m_testAct->setIcon(modern(QStringLiteral("test.png")));
        m_addAct->setIcon(classic(QStringLiteral("New.png")));
        m_deleteAct->setIcon(classic(QStringLiteral("Close.png")));
        m_closeAct->setIcon(classic(QStringLiteral("Close.png")));
        m_prefsAct->setIcon(classic(QStringLiteral("Preferences.png")));
        m_aboutAct->setIcon(classic(QStringLiteral("About.png")));
        m_quitAct->setIcon(classic(QStringLiteral("Quit.png")));
    }

    for (QAction *action : {m_openAct, m_newAct, m_extractAct, m_oneClickAct,
                            m_testAct, m_addAct, m_deleteAct, m_closeAct,
                            m_prefsAct, m_aboutAct, m_quitAct}) {
        if (!action) {
            continue;
        }
        QString tip = action->text();
        if (action->shortcut().isEmpty()) {
            action->setToolTip(tip);
        } else {
            tip += QStringLiteral(" (%1)")
                       .arg(action->shortcut().toString(QKeySequence::NativeText));
            action->setToolTip(tip);
        }
    }
}

void MainWindow::setupMenuBar()
{
    QMenuBar *bar = menuBar();
    bar->clear();
    bar->setNativeMenuBar(false);

    auto *fileMenu = bar->addMenu(tr("&File"));
    fileMenu->addAction(m_openAct);
    fileMenu->addAction(m_newAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_closeAct);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAct);

    auto *actionMenu = bar->addMenu(tr("&Actions"));
    actionMenu->addAction(m_extractAct);
    actionMenu->addAction(m_oneClickAct);
    actionMenu->addSeparator();
    actionMenu->addAction(m_addAct);
    actionMenu->addAction(m_deleteAct);
    actionMenu->addSeparator();
    actionMenu->addAction(m_testAct);

    auto *toolsMenu = bar->addMenu(tr("&Tools"));
    auto *benchmarkAction = toolsMenu->addAction(
        Icons::modern(QStringLiteral("toolbox.png")), tr("7-Zip Benchmark..."));
    connect(benchmarkAction, &QAction::triggered,
            this, &MainWindow::onBenchmark);
    toolsMenu->addAction(m_prefsAct);

    auto *helpMenu = bar->addMenu(tr("&Help"));
    helpMenu->addAction(m_aboutAct);

    bar->show();
}

void MainWindow::applyToolButtonStyle()
{
    if (!m_mainToolBar) {
        return;
    }
    const QString style = AppSettings::instance().toolButtonStyle();
    Qt::ToolButtonStyle buttonStyle = Qt::ToolButtonTextUnderIcon;
    if (style == QStringLiteral("textBesideIcon")) {
        buttonStyle = Qt::ToolButtonTextBesideIcon;
    } else if (style == QStringLiteral("textOnly")) {
        buttonStyle = Qt::ToolButtonTextOnly;
    } else if (style == QStringLiteral("iconOnly")) {
        buttonStyle = Qt::ToolButtonIconOnly;
    }
    m_mainToolBar->setToolButtonStyle(buttonStyle);
}

void MainWindow::applySidebarVisibility()
{
    if (m_sidebarDock) {
        m_sidebarDock->setVisible(AppSettings::instance().showRecentFiles());
    }
}

void MainWindow::updateActionsState()
{
    ArchiveBackend *backend = ArchiveManager::instance().backend();
    const bool busy = ArchiveManager::instance().isBusy();
    const bool hasArchive = (backend != nullptr);
    const bool canModify = hasArchive
        && ArchiveManager::instance().canModifyCurrentArchive();
    const bool hasSelection = !selectedEntryPaths().isEmpty();

    m_openAct->setEnabled(!busy);
    m_newAct->setEnabled(!busy);
    m_extractAct->setEnabled(hasArchive && !busy);
    m_oneClickAct->setEnabled(!busy);
    m_testAct->setEnabled(hasArchive && !busy);
    m_addAct->setEnabled(canModify && !busy);
    m_deleteAct->setEnabled(canModify && hasSelection && !busy);
    m_closeAct->setEnabled(hasArchive && !busy);
    m_prefsAct->setEnabled(!busy);
    m_aboutAct->setEnabled(true);
    m_quitAct->setEnabled(true);

    updateNavigationState();
}

void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Archive"), QString(), MainWindowHelpers::archiveDialogFilter());
    if (!path.isEmpty()) {
        openFile(path);
    }
}

void MainWindow::onNewArchive()
{
    const QStringList items = chooseSourceItems(tr("Select Files to Compress"));
    if (items.isEmpty()) {
        return;
    }

    CompressDialog dlg(items, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_currentOp = UiOp::Compress;
        showProgressDialog(tr("Creating Archive..."));
        ArchiveManager::instance().compress(dlg.options(), dlg.selectedFormat());
    }
}

void MainWindow::onTestArchive()
{
    if (!ArchiveManager::instance().backend()) {
        return;
    }
    TestOptions options;
    options.password = m_currentPassword;
    m_currentOp = UiOp::Test;
    showProgressDialog(tr("Testing Archive..."));
    ArchiveManager::instance().testArchive(options);
}

void MainWindow::onAddFiles()
{
    if (!ArchiveManager::instance().backend()) {
        return;
    }
    const QStringList items = chooseSourceItems(tr("Select Files to Add"));
    if (items.isEmpty()) {
        return;
    }
    addFilesToCurrentArchive(items);
}

void MainWindow::onDeleteSelected()
{
    ArchiveBackend *backend = ArchiveManager::instance().backend();
    const QStringList paths = selectedEntryPaths();
    if (!backend || paths.isEmpty()) {
        return;
    }
    if (!ArchiveManager::instance().canModifyCurrentArchive()) {
        QMessageBox::information(this, tr("Delete Files"),
                                 tr("This archive does not support deleting files."));
        return;
    }

    const auto answer = QMessageBox::question(
        this, tr("Delete Selected Entries"),
        tr("Delete %1 selected entr%2 from the archive?\n\nThis cannot be undone.")
            .arg(paths.size())
            .arg(paths.size() == 1 ? QStringLiteral("y") : QStringLiteral("ies")),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    DeleteFilesOptions options;
    options.archivePath = ArchiveManager::instance().currentPath();
    options.targetEntries = paths;
    options.password = m_currentPassword;

    m_currentOp = UiOp::Delete;
    showProgressDialog(tr("Deleting Entries..."));
    ArchiveManager::instance().deleteFiles(options);
}

void MainWindow::onPreferences()
{
    const QString oldLayout = AppSettings::instance().uiLayoutMode();
    PreferencesDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const QString newLayout = AppSettings::instance().uiLayoutMode();
    bool layoutChanged = newLayout != oldLayout;
    if (layoutChanged && ArchiveManager::instance().isBusy()) {
        // Rebuilding the views while an operation is running would drop the
        // progress dialog's view of the archive.  Keep the old layout.
        // 操作进行中重建视图会丢失进度对话框的上下文，因此保留旧布局。
        AppSettings::instance().setUiLayoutMode(oldLayout);
        ThemeManager::applyTheme(*qApp);
        MessageBoxUtils::showWarning(
            this, tr("Preferences"),
            tr("The layout can only be switched while no archive operation is "
               "running. The previous layout was kept."));
        layoutChanged = false;
    }

    if (!AppSettings::instance().rememberPasswords()) {
        m_sessionPassword.clear();
    }

    if (layoutChanged) {
        teardownUi();
        buildUi();
    } else {
        applyContentViewMode();
        applyToolButtonStyle();
        applySidebarVisibility();
        updateSearchIcon();
        updateStatusSummary();
        updateSelectionDetails();
        updateNavLocation();
        updateNavigationState();
    }
    updateActionsState();
}

void MainWindow::onBenchmark()
{
    BenchmarkDialog dlg(this);
    dlg.exec();
}

void MainWindow::onCloseArchive()
{
    ArchiveManager::instance().closeArchive();
    // Staged drag data belongs to the archive that is being closed.
    DragOutManager::instance().clearCache();
    m_model->setEntries({});
    m_listReady = false;
    m_oneClickPending = false;
    m_oneClickExtractActive = false;
    m_oneClickCloseAfter = false;
    setSearchText(QString());

    m_currentPassword.clear();
    if (!AppSettings::instance().rememberPasswords()) {
        m_sessionPassword.clear();
    }

    setBusyIndicatorVisible(false);
    setWindowTitle(tr("QArchiver"));
    resetNavigationHistory();
    if (m_folderPane) {
        m_folderPane->clear();
    }
    showPlaceholder(tr("No archive open.\nUse the Open button or drag a file here."));
    statusBar()->showMessage(tr("Archive closed"), 3000);
    updateStatusSummary();
    updateActionsState();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About QArchiver"),
        tr("<h3>QArchiver (小Q解压)</h3>"
           "<p>A lightweight Qt-based archive manager for Linux and Windows.</p>"
           "<p>The modern interface follows HaoZip: flat toolbar, folder and "
           "details panes, one-click extraction and the classic layout in "
           "Preferences. No ads, no bundled security scanners.</p>"
           "<p>Supports: tar, tar.gz, tar.xz, tar.bz2, zip, 7z, rar, gz, bz2, xz</p>"
           "<p>Version: %1</p>")
            .arg(QCoreApplication::applicationVersion()));
}
