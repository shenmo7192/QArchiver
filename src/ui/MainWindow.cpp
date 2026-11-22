// QArchiver / 小Q解压
// MainWindow implementation: modern and classic layouts, actions, drag/drop,
// previews, one-click extraction and dialogs.
// MainWindow 实现：现代与经典布局、动作、拖放、预览、一键解压与对话框。

#include "MainWindow.h"

#include "FileListModel.h"
#include "FilterProxyModel.h"
#include "FileListViewDelegate.h"
#include "PreviewManager.h"
#include "PreviewUtils.h"
#include "DragOutManager.h"
#include "widgets/ArchiveTreeView.h"
#include "widgets/ArchiveGridView.h"
#include "widgets/BackgroundWidget.h"
#include "widgets/SidebarWidget.h"
#include "widgets/ModernToolBar.h"
#include "widgets/ModernNavBar.h"
#include "widgets/ArchiveFolderPane.h"
#include "widgets/WelcomePage.h"
#include "dialogs/ExtractDialog.h"
#include "dialogs/CompressDialog.h"
#include "dialogs/PasswordDialog.h"
#include "dialogs/ProgressDialog.h"
#include "dialogs/PreferencesDialog.h"
#include "dialogs/BenchmarkDialog.h"
#include "utils/Logger.h"
#include "utils/MimeTypeHelper.h"
#include "utils/SevenZipTool.h"
#include "utils/MessageBoxUtils.h"
#include "utils/FileUtils.h"
#include "utils/FileTypeHelper.h"
#include "utils/ThemeManager.h"

#include "core/ArchiveManager.h"
#include "core/ArchiveError.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveConstants.h"
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

namespace {

QIcon resourceIcon(const QString &fileName)
{
    return QIcon(QStringLiteral(":/icons/") + fileName);
}

QIcon modernIcon(const QString &fileName)
{
    return QIcon(QStringLiteral(":/modern/") + fileName);
}

bool isArchiveFile(const QString &path)
{
    // Split RAR volumes may end in .r00/.r01/... instead of .rar.
    // 分卷 RAR 的后续卷可能以 .r00/.r01/... 结尾，而不是 .rar。
    if (MimeTypeHelper::isSplitVolume(path)) {
        return true;
    }

    static const QStringList suffixes = {
        QStringLiteral(".tar"), QStringLiteral(".tar.gz"),
        QStringLiteral(".tgz"), QStringLiteral(".tar.xz"),
        QStringLiteral(".txz"), QStringLiteral(".tar.bz2"),
        QStringLiteral(".tbz2"), QStringLiteral(".zip"),
        QStringLiteral(".7z"), QStringLiteral(".rar"),
        QStringLiteral(".gz"), QStringLiteral(".bz2"),
        QStringLiteral(".xz"), QStringLiteral(".001")
    };
    for (const QString &suffix : suffixes) {
        if (path.endsWith(suffix, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

// QFileDialog that allows selecting files and directories together.
// 允许同时选择文件和目录的 QFileDialog。
class MixedSelectionFileDialog : public QFileDialog
{
public:
    using QFileDialog::QFileDialog;

    void accept() override
    {
        // Skip QFileDialog's default "enter directory on Open" behavior so
        // the current selection is returned as the dialog result.
        QDialog::accept();
    }
};

QString archiveDialogFilter()
{
    return QCoreApplication::translate(
        "MainWindow",
        "Archives (*.tar *.tar.gz *.tgz *.tar.xz *.txz *.tar.bz2 *.tbz2 "
        "*.zip *.7z *.rar *.r?? *.gz *.bz2 *.xz *.001);;All Files (*)");
}

QString formatFileSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("QArchiver"));
    setWindowIcon(modernIcon(QStringLiteral("app-icon.png")));
    resize(1180, 760);
    setMinimumSize(880, 560);

    m_model = new FileListModel(this);
    m_proxyModel = new FilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    setupActions();
    m_modernUi = AppSettings::instance().uiLayoutMode() == QStringLiteral("modern");
    buildUi();

    // The background image is a presentation option, independent of the
    // archive data.  It is only used by the classic layout.
    // 背景图片属于展示选项，与压缩包数据无关。仅经典布局使用。
    connect(&AppSettings::instance(), &AppSettings::backgroundImageChanged,
            this, &MainWindow::updatePlaceholderBackground);

    auto &manager = ArchiveManager::instance();
    connect(&manager, &ArchiveManager::archiveOpening,
            this, &MainWindow::onArchiveOpening);
    connect(&manager, &ArchiveManager::archiveOpened,
            this, &MainWindow::onArchiveOpened);
    connect(&manager, &ArchiveManager::fileListReady,
            this, &MainWindow::onFileListReady);
    connect(&manager, &ArchiveManager::progressChanged,
            this, &MainWindow::onProgressChanged);
    connect(&manager, &ArchiveManager::outputReady,
            this, &MainWindow::onOutputReady);
    connect(&manager, &ArchiveManager::passwordRequested,
            this, &MainWindow::onPasswordRequested);
    connect(&manager, &ArchiveManager::errorOccurred,
            this, &MainWindow::onErrorOccurred);
    connect(&manager, &ArchiveManager::operationFinished,
            this, &MainWindow::onOperationFinished);

    manager.setSymlinkDecisionCallback(
        [this](const QString &path, const QString &target) {
            QMessageBox box(this);
            box.setWindowTitle(tr("Unsafe Symbolic Link"));
            box.setIconPixmap(resourceIcon(QStringLiteral("warning.ico"))
                                  .pixmap(32, 32));
            box.setText(tr("The archive contains a symbolic link that points "
                           "outside the extraction directory."));
            box.setInformativeText(
                tr("Link: %1\nTarget: %2\n\n"
                   "Skipping the link prevents it and its contents from being "
                   "extracted. Aborting removes the temporary extraction.")
                    .arg(path, target.isEmpty() ? tr("(empty)") : target));
            auto *applyAll = new QCheckBox(
                tr("Apply to the same items in this task"), &box);
            box.setCheckBox(applyAll);
            QPushButton *skipButton =
                box.addButton(tr("Skip Link"), QMessageBox::DestructiveRole);
            QPushButton *abortButton =
                box.addButton(tr("Abort Extraction"), QMessageBox::RejectRole);
            box.setDefaultButton(abortButton);
            box.exec();
            if (box.clickedButton() == skipButton) {
                return applyAll->isChecked()
                    ? ArchiveManager::SymlinkDecision::SkipAll
                    : ArchiveManager::SymlinkDecision::Skip;
            }
            return ArchiveManager::SymlinkDecision::Abort;
        });

    // Ctrl+F focuses the search field of whichever layout is active.
    auto *findAction = new QAction(this);
    findAction->setShortcut(QKeySequence::Find);
    addAction(findAction);
    connect(findAction, &QAction::triggered, this, [this]() {
        if (m_modernUi && m_modernNavBar) {
            m_modernNavBar->focusSearch();
        } else if (m_searchEdit) {
            m_searchEdit->setFocus(Qt::ShortcutFocusReason);
            m_searchEdit->selectAll();
        }
    });

    PreviewManager::instance().cleanupStale();
    DragOutManager::instance().cleanupStale();
    updateActionsState();

    // Probe the fallback CLI after the window is visible, so the warning is
    // shown once without delaying application startup.
    // 窗口显示后再检测兜底 CLI，只提示一次且不阻塞启动。
    QTimer::singleShot(0, this, &MainWindow::checkSevenZipFallback);
}

MainWindow::~MainWindow()
{
    ArchiveManager::instance().setSymlinkDecisionCallback(nullptr);
    PreviewManager::instance().cleanupOwn();
    DragOutManager::instance().cleanupOwn();
}

void MainWindow::openArchivePath(const QString &path)
{
    if (!path.isEmpty()) {
        openFile(path);
    }
}

void MainWindow::extractOneClickPath(const QString &path)
{
    m_oneClickPending = true;
    openFile(path);
}

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
        return resourceIcon(name);
    };
    const auto modern = [](const QString &name) {
        return modernIcon(name);
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

void MainWindow::buildUi()
{
    m_modernUi = AppSettings::instance().uiLayoutMode() == QStringLiteral("modern");
    applyActionIcons();

    if (m_modernUi) {
        setupModernUi();
    } else {
        setupClassicUi();
    }
    setupStatusBar();
    applyContentViewMode();

    if (ArchiveManager::instance().backend()) {
        showArchivePage();
        if (m_modernUi) {
            m_folderPane->setEntries(m_model->sourceEntries());
            m_folderPane->setCurrentDirectory(m_navCurrentDir);
        }
        updateStatusSummary();
    } else {
        if (m_modernUi) {
            showWelcomePage();
        } else {
            showPlaceholder(tr("No archive open.\nUse the Open button or drag a file here."));
        }
    }

    updateNavLocation();
    updateNavigationState();
    updateSelectionDetails();
    updateActionsState();
    if (!m_searchText.isEmpty()) {
        setSearchText(m_searchText);
    }
    QTimer::singleShot(0, this, &MainWindow::updateSearchIcon);
}

void MainWindow::teardownUi()
{
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }

    if (m_sidebarDock) {
        removeDockWidget(m_sidebarDock);
        delete m_sidebarDock;
        m_sidebarDock = nullptr;
    }

    const QList<QToolBar *> toolbars = findChildren<QToolBar *>();
    for (QToolBar *toolBar : toolbars) {
        removeToolBar(toolBar);
        delete toolBar;
    }
    m_mainToolBar = nullptr;

    if (auto *bar = menuBar()) {
        bar->clear();
        bar->hide();
    }

    QWidget *oldCentral = takeCentralWidget();
    if (oldCentral) {
        oldCentral->hide();
        delete oldCentral;
    }

    if (m_busyWidget) {
        statusBar()->removeWidget(m_busyWidget);
        delete m_busyWidget;
        m_busyWidget = nullptr;
        m_busyLabel = nullptr;
        m_busyBar = nullptr;
    }
    if (m_statusSummaryLeft) {
        statusBar()->removeWidget(m_statusSummaryLeft);
        delete m_statusSummaryLeft;
        m_statusSummaryLeft = nullptr;
    }
    if (m_statusSummaryRight) {
        statusBar()->removeWidget(m_statusSummaryRight);
        delete m_statusSummaryRight;
        m_statusSummaryRight = nullptr;
    }
    statusBar()->clearMessage();

    m_treeView = nullptr;
    m_gridView = nullptr;
    m_viewStack = nullptr;
    m_centralStack = nullptr;
    m_searchEdit = nullptr;
    m_placeholderLabel = nullptr;
    m_searchAction = nullptr;
    m_backgroundWidget = nullptr;
    m_modernToolBar = nullptr;
    m_modernNavBar = nullptr;
    m_folderPane = nullptr;
    m_welcomePage = nullptr;
    m_bodySplitter = nullptr;
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
        modernIcon(QStringLiteral("toolbox.png")), tr("7-Zip Benchmark..."));
    connect(benchmarkAction, &QAction::triggered,
            this, &MainWindow::onBenchmark);
    toolsMenu->addAction(m_prefsAct);

    auto *helpMenu = bar->addMenu(tr("&Help"));
    helpMenu->addAction(m_aboutAct);

    bar->show();
}

void MainWindow::setupModernUi()
{
    // The large toolbar and folder/details panes need a little more room than
    // the classic layout, especially with English labels.
    // 大号工具栏与文件夹/详细信息面板比经典布局需要更多空间，英文标签尤其如此。
    setMinimumSize(980, 600);
    setupMenuBar();

    auto *root = new QWidget(this);
    root->setObjectName(QStringLiteral("ModernRoot"));
    auto *layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_modernToolBar = new ModernToolBar(root);
    m_modernToolBar->addToolAction(m_openAct);
    m_modernToolBar->addToolAction(m_addAct);
    m_modernToolBar->addSeparator();
    m_modernToolBar->addToolAction(m_extractAct);
    m_modernToolBar->addToolAction(m_oneClickAct);
    m_modernToolBar->addToolAction(m_newAct);
    m_modernToolBar->addToolAction(m_deleteAct);
    m_modernToolBar->addToolAction(m_testAct);
    m_modernToolBar->addSeparator();
    m_modernToolBar->addToolAction(m_prefsAct);
    m_modernToolBar->addToolAction(m_aboutAct);
    layout->addWidget(m_modernToolBar);

    m_modernNavBar = new ModernNavBar(root);
    layout->addWidget(m_modernNavBar);

    m_bodySplitter = new QSplitter(Qt::Horizontal, root);
    m_bodySplitter->setChildrenCollapsible(false);
    m_bodySplitter->setHandleWidth(1);

    m_folderPane = new ArchiveFolderPane(m_bodySplitter);
    m_folderPane->setMinimumWidth(180);
    m_folderPane->setMaximumWidth(340);
    m_bodySplitter->addWidget(m_folderPane);

    auto *content = new QWidget(m_bodySplitter);
    content->setObjectName(QStringLiteral("ModernContent"));
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    m_centralStack = new QStackedWidget(content);
    m_centralStack->setObjectName(QStringLiteral("CentralStack"));
    m_centralStack->setFrameShape(QFrame::NoFrame);

    m_welcomePage = new WelcomePage(m_centralStack);

    auto *archivePage = new QWidget(m_centralStack);
    archivePage->setObjectName(QStringLiteral("ArchivePage"));
    auto *archiveLayout = new QVBoxLayout(archivePage);
    archiveLayout->setContentsMargins(0, 0, 0, 0);
    archiveLayout->setSpacing(0);
    m_viewStack = new QStackedWidget(archivePage);
    m_treeView = createArchiveTree(m_viewStack);
    m_gridView = createArchiveGrid(m_viewStack);
    m_viewStack->addWidget(m_treeView);
    m_viewStack->addWidget(m_gridView);
    archiveLayout->addWidget(m_viewStack, 1);

    m_centralStack->addWidget(m_welcomePage);
    m_centralStack->addWidget(archivePage);
    m_centralStack->setCurrentIndex(0);
    contentLayout->addWidget(m_centralStack, 1);

    m_bodySplitter->addWidget(content);
    m_bodySplitter->setStretchFactor(0, 0);
    m_bodySplitter->setStretchFactor(1, 1);
    m_bodySplitter->setSizes({230, 950});

    layout->addWidget(m_bodySplitter, 1);
    setCentralWidget(root);

    connect(m_modernNavBar, &ModernNavBar::backRequested,
            this, &MainWindow::onNavBack);
    connect(m_modernNavBar, &ModernNavBar::forwardRequested,
            this, &MainWindow::onNavForward);
    connect(m_modernNavBar, &ModernNavBar::upRequested,
            this, &MainWindow::onNavUp);
    connect(m_modernNavBar, &ModernNavBar::searchChanged,
            this, &MainWindow::onSearchChanged);
    connect(m_modernNavBar, &ModernNavBar::viewModeChanged,
            this, &MainWindow::onContentViewModeChanged);
    connect(m_folderPane, &ArchiveFolderPane::directoryActivated,
            this, &MainWindow::onFolderDirectoryActivated);
    connect(m_welcomePage, &WelcomePage::openRequested,
            this, &MainWindow::onOpen);
    connect(m_welcomePage, &WelcomePage::oneClickExtractRequested,
            this, &MainWindow::onOneClickExtract);
    connect(m_welcomePage, &WelcomePage::recentFileActivated,
            this, &MainWindow::onSidebarFileOpened);

    m_modernNavBar->setSearchText(m_searchText);
}

void MainWindow::setupClassicUi()
{
    setMinimumSize(880, 560);
    if (auto *bar = menuBar()) {
        bar->hide();
    }

    setupClassicToolbar();
    setupClassicCentralWidget();
    setupClassicSidebar();
    applyToolButtonStyle();
    applySidebarVisibility();
}

void MainWindow::setupClassicToolbar()
{
    auto *tb = addToolBar(tr("Main"));
    m_mainToolBar = tb;
    tb->setObjectName(QStringLiteral("MainToolBar"));
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tb->setIconSize(QSize(32, 32));

    tb->addAction(m_openAct);
    tb->addAction(m_newAct);

    tb->addSeparator();
    tb->addAction(m_extractAct);
    tb->addAction(m_oneClickAct);
    tb->addAction(m_testAct);
    tb->addAction(m_addAct);
    tb->addAction(m_deleteAct);

    tb->addSeparator();
    tb->addAction(m_closeAct);
    tb->addAction(m_prefsAct);
    tb->addAction(m_aboutAct);

    auto *spacer = new QWidget(this);
    spacer->setObjectName(QStringLiteral("ToolbarSpacer"));
    spacer->setAttribute(Qt::WA_TranslucentBackground);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    tb->addAction(m_quitAct);

    for (QAction *action : tb->actions()) {
        if (!action->isSeparator() && action->toolTip().isEmpty()) {
            action->setToolTip(action->text());
        }
    }
}

void MainWindow::setupClassicCentralWidget()
{
    m_backgroundWidget = new BackgroundWidget(this);
    m_centralStack = new QStackedWidget(m_backgroundWidget);
    m_centralStack->setObjectName(QStringLiteral("CentralStack"));
    m_centralStack->setFrameShape(QFrame::NoFrame);
    m_centralStack->setAutoFillBackground(false);

    // Placeholder page (shown when no archive is open)
    // 占位页（未打开压缩包时显示）
    auto *placeholder = new QWidget(m_centralStack);
    placeholder->setObjectName(QStringLiteral("PlaceholderPage"));
    placeholder->setAutoFillBackground(false);
    auto *placeholderLayout = new QVBoxLayout(placeholder);
    placeholderLayout->addStretch(1);
    m_placeholderLabel = new QLabel(
        tr("No archive open.\nUse the Open button or drag a file here."),
        placeholder);
    m_placeholderLabel->setObjectName(QStringLiteral("PlaceholderLabel"));
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    placeholderLayout->addWidget(m_placeholderLabel);
    auto *openBtn = new QPushButton(resourceIcon(QStringLiteral("Open.png")),
                                    tr("Choose Archive..."), placeholder);
    openBtn->setObjectName(QStringLiteral("PlaceholderOpenButton"));
    openBtn->setIconSize(QSize(16, 16));
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpen);
    placeholderLayout->addWidget(openBtn, 0, Qt::AlignHCenter);
    placeholderLayout->addStretch(1);

    // Archive page: search field + tree
    // 归档页：搜索框 + 树
    auto *archivePage = new QWidget(m_centralStack);
    archivePage->setObjectName(QStringLiteral("ArchivePage"));
    archivePage->setAutoFillBackground(false);
    auto *archiveLayout = new QVBoxLayout(archivePage);
    archiveLayout->setContentsMargins(0, 0, 0, 0);
    archiveLayout->setSpacing(2);

    m_searchEdit = new QLineEdit(archivePage);
    m_searchEdit->setObjectName(QStringLiteral("SearchEdit"));
    m_searchEdit->setPlaceholderText(tr("Filter by name or path (wildcards allowed)..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchAction = new QAction(QString(), m_searchEdit);
    m_searchAction->setEnabled(false);
    m_searchEdit->addAction(m_searchAction, QLineEdit::LeadingPosition);
    updateSearchIcon();
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onSearchChanged);
    archiveLayout->addWidget(m_searchEdit);

    m_viewStack = new QStackedWidget(archivePage);
    m_treeView = createArchiveTree(m_viewStack);
    m_gridView = createArchiveGrid(m_viewStack);
    m_viewStack->addWidget(m_treeView);
    m_viewStack->addWidget(m_gridView);
    archiveLayout->addWidget(m_viewStack, 1);

    m_centralStack->addWidget(placeholder);   // index 0
    m_centralStack->addWidget(archivePage);   // index 1
    m_centralStack->setCurrentIndex(0);

    auto *backgroundLayout = new QVBoxLayout(m_backgroundWidget);
    backgroundLayout->setContentsMargins(0, 0, 0, 0);
    backgroundLayout->setSpacing(0);
    backgroundLayout->addWidget(m_centralStack);
    setCentralWidget(m_backgroundWidget);

    connect(m_centralStack, &QStackedWidget::currentChanged,
            this, &MainWindow::updatePlaceholderBackground);
    m_treeView->reloadBackgroundFromSettings();
    updatePlaceholderBackground();
}

ArchiveTreeView *MainWindow::createArchiveTree(QWidget *parent)
{
    auto *tree = new ArchiveTreeView(parent);
    tree->setObjectName(QStringLiteral("ArchiveTree"));
    tree->setModel(m_proxyModel);
    tree->setItemDelegate(new FileListViewDelegate(tree));
    tree->setFrameShape(QFrame::NoFrame);
    tree->viewport()->setAutoFillBackground(false);
    connect(tree->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this, tree]() {
        if (tree == currentContentView()) {
            onSelectionChanged();
        }
    });
    auto *header = tree->header();
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    header->setStretchLastSection(false);
    header->setSectionsClickable(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setMinimumSectionSize(60);
    tree->setIconSize(QSize(20, 20));
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(tree, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);
    connect(tree, &ArchiveTreeView::entryDoubleClicked,
            this, &MainWindow::onEntryDoubleClicked);
    connect(tree, &ArchiveTreeView::dragOutRequested,
            this, &MainWindow::onEntriesDragOut);
    connect(&AppSettings::instance(), &AppSettings::backgroundImageChanged,
            tree, &ArchiveTreeView::reloadBackgroundFromSettings);

    applyHeaderColumns();
    tree->reloadBackgroundFromSettings();
    return tree;
}

ArchiveGridView *MainWindow::createArchiveGrid(QWidget *parent)
{
    auto *grid = new ArchiveGridView(parent);
    grid->setModel(m_proxyModel);
    connect(grid, &ArchiveGridView::entryActivated,
            this, &MainWindow::onEntryDoubleClicked);
    connect(grid, &ArchiveGridView::dragOutRequested,
            this, &MainWindow::onEntriesDragOut);
    connect(grid->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this, grid]() {
        if (grid == currentContentView()) {
            onSelectionChanged();
        }
    });
    connect(grid, &QWidget::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);
    return grid;
}

void MainWindow::setupClassicSidebar()
{
    m_sidebarDock = new QDockWidget(tr("Recent Files"), this);
    m_sidebarDock->setObjectName(QStringLiteral("SidebarDock"));
    m_sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto *sidebar = new SidebarWidget(m_sidebarDock);
    m_sidebarDock->setWidget(sidebar);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);
    connect(sidebar, &SidebarWidget::fileOpened,
            this, &MainWindow::onSidebarFileOpened);

    applySidebarVisibility();
}

void MainWindow::createBusyWidget()
{
    m_busyLabel = new QLabel(this);
    m_busyLabel->setObjectName(QStringLiteral("BusyLabel"));
    m_busyBar = new QProgressBar(this);
    m_busyBar->setObjectName(QStringLiteral("BusyProgress"));
    m_busyBar->setRange(0, 0);
    m_busyBar->setFixedWidth(120);
    m_busyBar->setTextVisible(false);
    m_busyWidget = new QWidget(this);
    m_busyWidget->setObjectName(QStringLiteral("BusyWidget"));
    auto *busyLayout = new QHBoxLayout(m_busyWidget);
    busyLayout->setContentsMargins(0, 0, 0, 0);
    busyLayout->setSpacing(6);
    busyLayout->addWidget(m_busyLabel);
    busyLayout->addWidget(m_busyBar);
    m_busyWidget->hide();
}

void MainWindow::setupStatusBar()
{
    statusBar()->setObjectName(QStringLiteral("MainStatusBar"));
    createBusyWidget();

    if (m_modernUi) {
        m_statusSummaryLeft = new QLabel(this);
        m_statusSummaryLeft->setObjectName(QStringLiteral("StatusSummaryLeft"));
        m_statusSummaryRight = new QLabel(this);
        m_statusSummaryRight->setObjectName(QStringLiteral("StatusSummaryRight"));
        statusBar()->addWidget(m_statusSummaryLeft, 1);
        statusBar()->addPermanentWidget(m_busyWidget);
        statusBar()->addPermanentWidget(m_statusSummaryRight);
    } else {
        statusBar()->addPermanentWidget(m_busyWidget);
    }
    statusBar()->showMessage(tr("Ready"), 2000);
}

QString MainWindow::contentViewMode() const
{
    const QString mode = AppSettings::instance().contentViewMode();
    if (mode == QStringLiteral("list") || mode == QStringLiteral("tree")) {
        return mode;
    }
    return QStringLiteral("grid");
}

QAbstractItemView *MainWindow::currentContentView() const
{
    if (contentViewMode() == QStringLiteral("grid") && m_gridView) {
        return m_gridView;
    }
    return m_treeView;
}

void MainWindow::applyContentViewMode()
{
    if (!m_treeView || !m_model) {
        return;
    }
    const QString mode = contentViewMode();
    const bool flat = mode != QStringLiteral("tree");
    const bool modeChanged = m_model->isFlatMode() != flat;
    if (modeChanged) {
        m_model->setFlatMode(flat);
        m_treeView->setFlatMode(flat);
    }
    applyHeaderColumns();

    const int sortSection = m_treeView->header()->sortIndicatorSection();
    const Qt::SortOrder sortOrder = m_treeView->header()->sortIndicatorOrder();
    m_treeView->sortByColumn(
        sortSection >= 0 ? sortSection : FileListModel::NameColumn,
        sortSection >= 0 ? sortOrder : Qt::AscendingOrder);

    if (m_viewStack && m_gridView) {
        QWidget *active = (mode == QStringLiteral("grid"))
            ? static_cast<QWidget *>(m_gridView)
            : static_cast<QWidget *>(m_treeView);
        m_viewStack->setCurrentWidget(active);
    }

    if (mode == QStringLiteral("tree")) {
        m_treeView->expandToDepth(0);
    } else if (mode == QStringLiteral("grid") && m_gridView) {
        m_gridView->scrollToTop();
    } else {
        m_treeView->scrollToTop();
    }

    if (m_modernUi) {
        if (modeChanged || m_navHistory.isEmpty()) {
            resetNavigationHistory(m_model->currentDirectory());
        }
        if (m_folderPane) {
            m_folderPane->setCurrentDirectory(m_navCurrentDir);
        }
        if (m_modernNavBar) {
            m_modernNavBar->setViewMode(mode);
        }
    }
    updateStatusSummary();
    updateSelectionDetails();
}

void MainWindow::applyHeaderColumns()
{
    if (!m_treeView) {
        return;
    }
    QHeaderView *header = m_treeView->header();
    header->setSectionHidden(FileListModel::TypeColumn, !m_modernUi);
    header->setSectionHidden(FileListModel::EncryptedColumn, m_modernUi);

    if (m_modernUi) {
        header->setFixedHeight(34);
        header->setStretchLastSection(false);
        header->setSectionResizeMode(FileListModel::NameColumn,
                                     QHeaderView::Stretch);
        header->resizeSection(FileListModel::SizeColumn, 100);
        header->resizeSection(FileListModel::PackedSizeColumn, 110);
        header->resizeSection(FileListModel::TypeColumn, 150);
        header->resizeSection(FileListModel::ModifiedColumn, 150);
    } else {
        header->setFixedHeight(24);
        // Keep the classic Explorer-like layout but let the last column fill
        // the remaining width instead of leaving a blank strip.
        // 保留经典布局，但让最后一列填满剩余宽度，避免右侧空白。
        header->setStretchLastSection(true);
        header->setSectionResizeMode(FileListModel::NameColumn,
                                     QHeaderView::Interactive);
        header->resizeSection(FileListModel::NameColumn, 360);
        header->resizeSection(FileListModel::SizeColumn, 100);
        header->resizeSection(FileListModel::PackedSizeColumn, 100);
        header->resizeSection(FileListModel::ModifiedColumn, 150);
        header->resizeSection(FileListModel::EncryptedColumn, 90);
    }
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

void MainWindow::updateSearchIcon()
{
    if (!m_searchAction) {
        return;
    }
    const bool dark = qApp->property("aeroDark").toBool();
    if (m_modernUi) {
        m_searchAction->setIcon(modernIcon(dark
            ? QStringLiteral("search-light.png")
            : QStringLiteral("search.png")));
    } else {
        m_searchAction->setIcon(QIcon(dark
            ? QStringLiteral(":/styles/search-dark.png")
            : QStringLiteral(":/styles/search.png")));
    }
}

void MainWindow::updatePlaceholderBackground()
{
    if (!m_backgroundWidget || !m_centralStack) {
        return;
    }

    if (m_centralStack->currentIndex() == 0) {
        m_backgroundWidget->reloadFromSettings();
    } else {
        // Archive page: ArchiveTreeView paints the image inside its viewport.
        // 归档页：由 ArchiveTreeView 在其 viewport 内绘制图片。
        m_backgroundWidget->setBackgroundImage(BackgroundImageOptions{});
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

QList<ArchiveEntry> MainWindow::selectedEntries() const
{
    QList<ArchiveEntry> entries;
    QAbstractItemView *view = currentContentView();
    if (!view || !view->selectionModel() || !m_model || !m_proxyModel) {
        return entries;
    }
    const QModelIndexList rows = view->selectionModel()->selectedRows();
    for (const QModelIndex &proxyIndex : rows) {
        if (proxyIndex.data(FileListModel::IsParentRole).toBool()) {
            continue;
        }
        const QModelIndex sourceIndex = m_proxyModel->mapToSource(proxyIndex);
        if (const ArchiveEntry *entry = m_model->entryAt(sourceIndex)) {
            entries.append(*entry);
        }
    }
    return entries;
}

void MainWindow::updateSelectionDetails()
{
    if (!m_modernUi) {
        return;
    }

    const QList<ArchiveEntry> entries = selectedEntries();
    if (m_folderPane) {
        m_folderPane->setSelection(entries);
    }

    if (!m_model || !m_model->isFlatMode()) {
        QString dir = m_navCurrentDir;
        if (entries.size() == 1) {
            QString path = FileUtils::normalizeEntryPath(entries.first().path);
            while (path.endsWith(QLatin1Char('/'))) {
                path.chop(1);
            }
            if (entries.first().isDir) {
                dir = path;
            } else {
                const int slash = path.lastIndexOf(QLatin1Char('/'));
                dir = slash < 0 ? QString() : path.left(slash);
            }
        }
        if (dir != m_navCurrentDir) {
            m_navCurrentDir = dir;
            if (m_folderPane) {
                m_folderPane->setCurrentDirectory(dir);
            }
            updateNavLocation();
            updateNavigationState();
        }
    }
}

void MainWindow::updateStatusSummary()
{
    if (!m_statusSummaryLeft || !m_statusSummaryRight) {
        return;
    }

    ArchiveBackend *backend = ArchiveManager::instance().backend();
    if (!backend) {
        m_statusSummaryLeft->setText(tr("No archive open"));
        m_statusSummaryRight->clear();
        return;
    }

    const QList<ArchiveEntry> entries = m_model->sourceEntries();
    qint64 unpacked = 0;
    qint64 packed = 0;
    int files = 0;
    int folders = 0;
    for (const ArchiveEntry &entry : entries) {
        if (entry.isDir) {
            ++folders;
            continue;
        }
        ++files;
        if (entry.size > 0
            && unpacked <= std::numeric_limits<qint64>::max() - entry.size) {
            unpacked += entry.size;
        }
        if (entry.packedSize > 0
            && packed <= std::numeric_limits<qint64>::max() - entry.packedSize) {
            packed += entry.packedSize;
        }
    }

    const QString format = FileTypeHelper::formatLabel(
        ArchiveManager::instance().currentFormat());
    if (entries.isEmpty()) {
        m_statusSummaryLeft->setText(
            tr("%1 · Reading archive...").arg(format));
    } else if (packed > 0 && unpacked > 0) {
        const double ratio =
            (1.0 - static_cast<double>(packed) / static_cast<double>(unpacked))
            * 100.0;
        m_statusSummaryLeft->setText(
            tr("%1 · Unpacked %2 · Ratio %3%")
                .arg(format, formatFileSize(unpacked))
                .arg(ratio, 0, 'f', 1));
    } else {
        // libarchive does not always expose packed sizes; do not claim 100%.
        // libarchive 未必提供压缩后大小；不要因此显示 100%。
        m_statusSummaryLeft->setText(
            tr("%1 · Unpacked %2").arg(format, formatFileSize(unpacked)));
    }
    m_statusSummaryRight->setText(
        tr("%1 files, %2 folders (%3)")
            .arg(files)
            .arg(folders)
            .arg(formatFileSize(unpacked)));
}

void MainWindow::updateNavLocation()
{
    if (!m_modernNavBar) {
        return;
    }
    const QString archivePath = ArchiveManager::instance().currentPath();
    if (archivePath.isEmpty()) {
        m_modernNavBar->setLocation(tr("No archive open"), QString());
        return;
    }
    m_modernNavBar->setLocation(QFileInfo(archivePath).fileName(),
                                m_navCurrentDir);
}

void MainWindow::updateNavigationState()
{
    if (!m_modernNavBar) {
        return;
    }
    const bool hasArchive = ArchiveManager::instance().backend() != nullptr;
    bool canUp = false;
    if (hasArchive) {
        if (m_model && m_model->isFlatMode()) {
            canUp = !m_model->currentDirectory().isEmpty();
        } else {
            canUp = !m_navCurrentDir.isEmpty();
        }
    }
    m_modernNavBar->setNavigationState(
        m_navIndex > 0,
        m_navIndex >= 0 && m_navIndex + 1 < m_navHistory.size(),
        canUp);
}

void MainWindow::resetNavigationHistory(const QString &directory)
{
    m_navHistory.clear();
    m_navHistory << directory;
    m_navIndex = 0;
    m_navCurrentDir = directory;
    updateNavLocation();
    updateNavigationState();
}

void MainWindow::navigateHistory(int delta)
{
    const int target = m_navIndex + delta;
    if (target < 0 || target >= m_navHistory.size()) {
        return;
    }
    const QString directory = m_navHistory.at(target);
    navigateToDirectory(directory, false);
    if (m_navCurrentDir == directory) {
        m_navIndex = target;
        updateNavigationState();
    }
}

void MainWindow::navigateToDirectory(const QString &path, bool addHistory)
{
    if (!m_model || !m_treeView) {
        return;
    }

    QString directory = FileUtils::normalizeEntryPath(path);
    if (directory == QStringLiteral(".")) {
        directory.clear();
    }

    if (m_model->isFlatMode()) {
        if (!m_model->enterDirectory(directory)) {
            if (directory != m_model->currentDirectory()) {
                return;
            }
        }
        m_navCurrentDir = m_model->currentDirectory();
        if (QAbstractItemView *view = currentContentView()) {
            view->scrollToTop();
        }
    } else {
        if (directory.isEmpty()) {
            m_treeView->clearSelection();
            m_treeView->setCurrentIndex(QModelIndex());
            m_treeView->scrollToTop();
            m_navCurrentDir.clear();
        } else {
            const QModelIndex index = m_model->indexForPath(directory);
            if (!index.isValid()) {
                return;
            }
            for (QModelIndex parent = index.parent(); parent.isValid();
                 parent = parent.parent()) {
                m_treeView->expand(parent);
            }
            m_treeView->setCurrentIndex(index);
            if (m_treeView->selectionModel()) {
                m_treeView->selectionModel()->select(
                    index, QItemSelectionModel::ClearAndSelect
                               | QItemSelectionModel::Rows);
            }
            m_treeView->scrollTo(index);
            m_navCurrentDir = directory;
        }
    }

    if (addHistory) {
        if (m_navHistory.isEmpty()) {
            m_navHistory << m_navCurrentDir;
            m_navIndex = 0;
        } else if (m_navHistory.value(m_navIndex) != m_navCurrentDir) {
            while (m_navHistory.size() > m_navIndex + 1) {
                m_navHistory.removeLast();
            }
            m_navHistory << m_navCurrentDir;
            m_navIndex = m_navHistory.size() - 1;
        }
    }

    setSearchText(QString());
    if (m_folderPane) {
        m_folderPane->setCurrentDirectory(m_navCurrentDir);
    }
    updateNavLocation();
    updateNavigationState();
    updateSelectionDetails();
}

void MainWindow::setSearchText(const QString &text)
{
    m_searchText = text;
    if (m_modernNavBar) {
        m_modernNavBar->setSearchText(text);
    }
    if (m_searchEdit) {
        const bool blocked = m_searchEdit->blockSignals(true);
        m_searchEdit->setText(text);
        m_searchEdit->blockSignals(blocked);
    }
    if (m_proxyModel) {
        m_proxyModel->setFilterWildcard(text.trimmed());
    }
}

void MainWindow::showPlaceholder(const QString &message)
{
    if (m_modernUi) {
        showWelcomePage();
        return;
    }
    if (m_placeholderLabel) {
        m_placeholderLabel->setText(message);
    }
    if (m_centralStack) {
        m_centralStack->setCurrentIndex(0);
    }
    updatePlaceholderBackground();
}

void MainWindow::showWelcomePage()
{
    if (!m_centralStack) {
        return;
    }
    m_centralStack->setCurrentIndex(0);
    if (m_welcomePage) {
        m_welcomePage->refreshRecent();
    }
    if (m_modernNavBar) {
        m_modernNavBar->setLocation(tr("No archive open"), QString());
    }
    updateNavigationState();
}

void MainWindow::showArchivePage()
{
    if (m_centralStack) {
        m_centralStack->setCurrentIndex(1);
    }
}

void MainWindow::showProgressDialog(const QString &title)
{
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
    auto *dlg = new ProgressDialog(this);
    dlg->setOperation(title);
    connect(dlg, &ProgressDialog::cancelled, this, []() {
        ArchiveManager::instance().cancelCurrentOperation();
    });
    m_progressDialog = dlg;
    dlg->show();
    QTimer::singleShot(0, this, &MainWindow::updateActionsState);
}

void MainWindow::setBusyIndicatorVisible(bool visible, const QString &text)
{
    if (!m_busyWidget) {
        return;
    }
    m_busyLabel->setText(text);
    m_busyWidget->setVisible(visible);
}

QStringList MainWindow::selectedEntryPaths() const
{
    if (auto *grid = qobject_cast<ArchiveGridView *>(currentContentView())) {
        return grid->selectedEntryPaths();
    }
    return m_treeView ? m_treeView->selectedEntryPaths() : QStringList();
}

void MainWindow::updateFolderPaneState()
{
    if (!m_folderPane) {
        return;
    }
    m_folderPane->setEntries(m_model->sourceEntries());
    m_folderPane->setCurrentDirectory(m_navCurrentDir);
    m_folderPane->setSelection(selectedEntries());
}

QStringList MainWindow::chooseSourceItems(const QString &title)
{
    // Native file dialogs cannot select files and directories in one dialog.
    // Ask which kind to pick, then use the system dialog for that kind.  This
    // keeps the common path native (Windows Explorer / GTK / KDE) instead of
    // forcing the Qt widget dialog on every desktop.
    // 原生文件对话框无法同时选择文件与目录；先询问类型，再调用系统对话框，
    // 避免为了混合选择而在所有桌面强制使用 Qt 控件对话框。
    QMessageBox chooser(this);
    chooser.setWindowTitle(title);
    chooser.setIcon(QMessageBox::Question);
    chooser.setText(tr("What would you like to select?"));
    chooser.setInformativeText(
        tr("Files can be selected together; folders are added one at a time."));
    QPushButton *filesButton = chooser.addButton(
        tr("Select Files..."), QMessageBox::AcceptRole);
    QPushButton *folderButton = chooser.addButton(
        tr("Select Folder..."), QMessageBox::ActionRole);
    chooser.addButton(QMessageBox::Cancel);
    chooser.setDefaultButton(filesButton);
    chooser.exec();

    if (chooser.clickedButton() == filesButton) {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, title, QDir::homePath(), tr("All Files (*)"));
        return files;
    }
    if (chooser.clickedButton() == folderButton) {
        const QString folder = QFileDialog::getExistingDirectory(
            this, title, QDir::homePath(),
            QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        return folder.isEmpty() ? QStringList() : QStringList{folder};
    }
    return {};
}

void MainWindow::openFile(const QString &path)
{
    // Keep the current archive (and its password) until the new one opens
    // successfully.  onArchiveOpened() switches to the session password.
    // 在新压缩包成功打开前保留当前压缩包及其密码。
    ArchiveManager::instance().openArchive(path);
    updateActionsState();
}

void MainWindow::startExtract(const ExtractOptions &opts)
{
    ExtractOptions options = opts;
    if (!m_currentPassword.isEmpty()) {
        options.password = m_currentPassword;
    }

    if (options.overwrite == OverwritePolicy::Ask) {
        const QDir dest(options.destDir);
        const bool hasExisting = dest.exists()
            && !dest.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
        if (!hasExisting) {
            options.overwrite = OverwritePolicy::Always;
        } else {
            const auto answer = QMessageBox::question(
                this, tr("Overwrite Files"),
                tr("The destination directory is not empty. Overwrite existing files?"),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                QMessageBox::No);
            if (answer == QMessageBox::Cancel) {
                return;
            }
            options.overwrite = (answer == QMessageBox::Yes)
                ? OverwritePolicy::Always
                : OverwritePolicy::Never;
        }
    }

    m_openDestAfterExtract = options.openDestAfterFinish;
    m_lastExtractDest = options.destDir;
    m_currentOp = UiOp::Extract;

    ArchiveManager::instance().extract(options);
    if (ArchiveManager::instance().isBusy()) {
        showProgressDialog(tr("Extracting..."));
    }
}

void MainWindow::addFilesToCurrentArchive(const QStringList &files)
{
    ArchiveBackend *backend = ArchiveManager::instance().backend();
    if (!backend || files.isEmpty()) {
        return;
    }
    if (!ArchiveManager::instance().canModifyCurrentArchive()) {
        QMessageBox::information(this, tr("Add Files"),
                                 tr("This archive does not support adding files."));
        return;
    }

    AddFilesOptions options;
    options.archivePath = ArchiveManager::instance().currentPath();
    options.sourceFiles = files;
    options.password = m_currentPassword;
    options.compressionLevel = AppSettings::instance().defaultCompressionLevel();
    options.followSymlinks = AppSettings::instance().followSymlinks();

    m_currentOp = UiOp::Add;
    showProgressDialog(tr("Adding Files..."));
    ArchiveManager::instance().addFiles(options);
}

// Slots

void MainWindow::onOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Open Archive"), QString(), archiveDialogFilter());
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

void MainWindow::onExtractAll()
{
    if (!ArchiveManager::instance().backend()) {
        return;
    }

    ExtractDialog dlg(ArchiveManager::instance().currentPath(), this);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    startExtract(dlg.options());
}

void MainWindow::onExtractSelected()
{
    const QStringList paths = selectedEntryPaths();
    if (paths.isEmpty() || !ArchiveManager::instance().backend()) {
        return;
    }

    ExtractDialog dlg(ArchiveManager::instance().currentPath(), this);
    dlg.setSelectedFiles(paths);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    startExtract(dlg.options());
}

QString MainWindow::oneClickArchiveBaseName(const QString &archivePath)
{
    QString name = QFileInfo(archivePath).fileName();
    static const QStringList suffixes = {
        QStringLiteral(".tar.gz"), QStringLiteral(".tar.xz"),
        QStringLiteral(".tar.bz2"), QStringLiteral(".tar.zst"),
        QStringLiteral(".tgz"), QStringLiteral(".txz"),
        QStringLiteral(".tbz2"), QStringLiteral(".tar"),
        QStringLiteral(".zip"), QStringLiteral(".7z"),
        QStringLiteral(".rar"), QStringLiteral(".gz"),
        QStringLiteral(".bz2"), QStringLiteral(".xz"),
        QStringLiteral(".001"), QStringLiteral(".part1")
    };
    for (const QString &suffix : suffixes) {
        if (name.endsWith(suffix, Qt::CaseInsensitive)) {
            name.chop(suffix.size());
            break;
        }
    }
    // Split-volume names resolve to "archive.7z" / "archive.zip" after the
    // numeric suffix was removed; drop the inner extension as well.
    // 分卷名去掉数字后缀后可能变成 "archive.7z" / "archive.zip"，
    // 再移除内层扩展名。
    if (name.endsWith(QStringLiteral(".7z"), Qt::CaseInsensitive)
        || name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        name.chop(3);
    }
    if (name.isEmpty()) {
        name = QStringLiteral("archive");
    }
    return name;
}

QString MainWindow::oneClickDestinationRoot() const
{
    const AppSettings &settings = AppSettings::instance();
    QString root;
    if (settings.oneClickExtractDestMode() == QStringLiteral("custom")) {
        root = settings.oneClickExtractDest();
    } else {
        root = QFileInfo(ArchiveManager::instance().currentPath()).absolutePath();
    }
    if (root.isEmpty()) {
        root = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);
    }
    if (root.isEmpty()) {
        root = QDir::homePath();
    }
    return QDir::cleanPath(root);
}

bool MainWindow::askOneClickClosePreference()
{
    AppSettings &settings = AppSettings::instance();
    if (settings.oneClickCloseAsked()) {
        return settings.oneClickCloseAfterExtract();
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("One-click Extract"));
    box.setIconPixmap(modernIcon(QStringLiteral("oneclick.png")).pixmap(48, 48));
    box.setText(tr("Close QArchiver after one-click extraction finishes?"));
    box.setInformativeText(
        tr("The archive is extracted to its own folder. You can change this "
           "behaviour later in Preferences → One-click Extract."));
    QPushButton *closeButton = box.addButton(tr("Extract and Close"),
                                             QMessageBox::AcceptRole);
    box.addButton(tr("Keep Window Open"), QMessageBox::RejectRole);
    // Emphasise the recommended "extract and close" choice with the accent
    // colour; QMessageBox styles the default button as primary.
    // 用强调色突出推荐的“解压后关闭”；QMessageBox 会把默认按钮画成主按钮。
    box.setDefaultButton(closeButton);
    // QMessageBox overrides its own minimumWidth from the layout; widen the
    // two labels instead so the informative text does not wrap into a column.
    // QMessageBox 会用布局覆盖自身 minimumWidth；改为加宽文本标签，
    // 避免说明文字被挤成窄列。
    box.setStyleSheet(
        "QLabel#qt_msgbox_label, QLabel#qt_msgbox_informativelabel "
        "{ min-width: 460px; }");
    box.exec();

    const bool closeAfter = box.clickedButton() == closeButton;
    settings.setOneClickCloseAfterExtract(closeAfter);
    settings.setOneClickCloseAsked(true);
    return closeAfter;
}

void MainWindow::beginOneClickExtract()
{
    auto &manager = ArchiveManager::instance();
    if (!manager.backend() || manager.isBusy()) {
        return;
    }
    if (!m_listReady) {
        m_oneClickPending = true;
        statusBar()->showMessage(tr("Reading archive contents..."), 4000);
        return;
    }

    m_oneClickPending = false;
    const bool closeAfter = askOneClickClosePreference();

    const QString root = oneClickDestinationRoot();
    if (root.isEmpty() || !QDir().mkpath(root)) {
        MessageBoxUtils::showWarning(
            this, tr("One-click Extract"),
            tr("Cannot create the destination directory:\n%1").arg(root));
        return;
    }
    if (!QFileInfo(root).isDir() || !QFileInfo(root).isWritable()) {
        MessageBoxUtils::showWarning(
            this, tr("One-click Extract"),
            tr("The destination directory is not writable:\n%1").arg(root));
        return;
    }

    const QString archivePath = manager.currentPath();
    const QString baseSource = MimeTypeHelper::isSplitVolume(archivePath)
        ? MimeTypeHelper::resolveFirstVolume(archivePath)
        : archivePath;
    const QString baseName = oneClickArchiveBaseName(baseSource);
    QString destination = QDir(root).filePath(baseName);
    if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink()) {
        destination = FileUtils::uniqueDestinationPath(destination);
    }

    ExtractOptions options;
    options.destDir = destination;
    options.preservePaths = true;
    options.overwrite = OverwritePolicy::Always;
    options.password = m_currentPassword;
    options.openDestAfterFinish = false;

    m_oneClickExtractActive = true;
    m_oneClickCloseAfter = closeAfter;
    m_openDestAfterExtract = AppSettings::instance().openDestAfterFinish()
        && !closeAfter;
    m_lastExtractDest = destination;
    m_currentOp = UiOp::Extract;

    statusBar()->showMessage(
        tr("One-click extracting to %1").arg(destination), 8000);
    manager.extract(options);
    if (manager.isBusy()) {
        showProgressDialog(tr("One-click Extract..."));
    }
    updateActionsState();
}

void MainWindow::onOneClickExtract()
{
    if (ArchiveManager::instance().isBusy()) {
        return;
    }

    if (ArchiveManager::instance().backend()) {
        m_oneClickPending = true;
        if (m_listReady) {
            beginOneClickExtract();
        } else {
            statusBar()->showMessage(tr("Reading archive contents..."), 4000);
        }
        return;
    }

    const QString path = QFileDialog::getOpenFileName(
        this, tr("One-click Extract"), QString(), archiveDialogFilter());
    if (path.isEmpty()) {
        return;
    }
    m_oneClickPending = true;
    openFile(path);
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

void MainWindow::onArchiveOpening(const QString &filePath)
{
    // A normal open invalidates the current list.  A write-through reopen
    // (add/delete on a read-only libarchive backend) reopens the same archive,
    // so keep the current list usable if the write is cancelled or fails.
    // 普通打开会使当前列表失效；只读 libarchive 后端的添加/删除写回退会重新
    // 打开同一个压缩包，因此保留当前列表，写操作取消/失败时仍可使用。
    if (m_currentOp != UiOp::Add && m_currentOp != UiOp::Delete) {
        m_listReady = false;
    }
    setBusyIndicatorVisible(true,
        tr("Detecting format: %1").arg(QFileInfo(filePath).fileName()));
    statusBar()->showMessage(tr("Detecting archive format..."));
    if (m_modernNavBar) {
        m_modernNavBar->setLocation(
            tr("Opening %1...").arg(QFileInfo(filePath).fileName()), QString());
    }
    updateActionsState();
}

void MainWindow::onArchiveOpened(const QString &filePath)
{
    m_currentPassword = m_sessionPassword;
    m_listReady = false;

    // A new archive means new content: drop staging prepared for the previous
    // one.  (The staging directories themselves are removed on exit.)
    DragOutManager::instance().clearCache();
    m_model->setEntries({});
    setSearchText(QString());
    setWindowTitle(tr("QArchiver - %1").arg(QFileInfo(filePath).fileName()));
    showArchivePage();
    resetNavigationHistory();
    if (m_folderPane) {
        m_folderPane->clear();
    }
    statusBar()->showMessage(tr("Opened: %1").arg(filePath), 3000);
    setBusyIndicatorVisible(true,
        tr("Reading archive %1").arg(QFileInfo(filePath).fileName()));

    ListOptions opts;
    if (!m_currentPassword.isEmpty()) {
        opts.password = m_currentPassword;
    }
    m_currentOp = UiOp::List;
    ArchiveManager::instance().listContents(opts);
    updateActionsState();
}

void MainWindow::onFileListReady(const QList<ArchiveEntry> &entries)
{
    if (m_currentOp == UiOp::List) {
        m_currentOp = UiOp::None;
    }
    m_listReady = true;
    m_model->setEntries(entries);
    if (!m_model->isFlatMode()) {
        m_treeView->expandToDepth(0);
    } else {
        m_navCurrentDir = m_model->currentDirectory();
    }
    if (m_modernUi) {
        updateFolderPaneState();
        if (m_navHistory.isEmpty()) {
            resetNavigationHistory(m_navCurrentDir);
        } else {
            updateNavLocation();
            updateNavigationState();
        }
    }
    setBusyIndicatorVisible(false);
    statusBar()->showMessage(tr("%1 entries").arg(entries.size()), 3000);
    updateStatusSummary();
    updateSelectionDetails();
    updateActionsState();

    if (m_oneClickPending) {
        beginOneClickExtract();
    }
}

void MainWindow::onProgressChanged(int percent, int total, int current, int etaSec)
{
    if (m_progressDialog) {
        m_progressDialog->updateProgress(percent, total, current, etaSec);
    }
}

void MainWindow::onOutputReady(const QString &line)
{
    if (m_progressDialog) {
        m_progressDialog->appendOutput(line);
    }
}

void MainWindow::onPasswordRequested()
{
    setBusyIndicatorVisible(false);

    PasswordDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) {
        ArchiveManager::instance().cancelPendingOperation();
        updateActionsState();
        return;
    }

    m_currentPassword = dlg.password();
    if (dlg.rememberPassword()) {
        m_sessionPassword = m_currentPassword;
    }
    setBusyIndicatorVisible(true, tr("Processing..."));
    ArchiveManager::instance().retryWithPassword(m_currentPassword);
}

void MainWindow::onErrorOccurred(ArchiveErrorCode code, const QString &msg)
{
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
    m_currentOp = UiOp::None;
    m_oneClickExtractActive = false;
    m_oneClickCloseAfter = false;
    m_oneClickPending = false;
    if (!ArchiveManager::instance().backend()) {
        m_listReady = false;
    }
    setBusyIndicatorVisible(false);

    if (code == ArchiveErrorCode::WrongPassword) {
        m_currentPassword.clear();
    }

    if (code == ArchiveErrorCode::Cancelled) {
        statusBar()->showMessage(msg.isEmpty() ? tr("Cancelled") : msg, 5000);
        if (ArchiveManager::instance().backend()) {
            m_listReady = true;
        }
        updateNavLocation();
        updateNavigationState();
        updateActionsState();
        return;
    }

    if (code == ArchiveErrorCode::PartialExtraction
        || code == ArchiveErrorCode::HardLinkUnsupported) {
        MessageBoxUtils::showWarning(
            this,
            code == ArchiveErrorCode::PartialExtraction
                ? tr("Extraction Incomplete")
                : tr("Hard Link Not Supported"),
            msg);
        updateActionsState();
        updateStatusSummary();
        return;
    }

    QString title = tr("Error");
    switch (code) {
    case ArchiveErrorCode::UnsupportedFormat:   title = tr("Unsupported Format"); break;
    case ArchiveErrorCode::WrongPassword:       title = tr("Wrong Password"); break;
    case ArchiveErrorCode::CorruptedArchive:    title = tr("Corrupted Archive"); break;
    case ArchiveErrorCode::ToolNotFound:        title = tr("Required Tool Not Found"); break;
    case ArchiveErrorCode::OperationNotSupported: title = tr("Operation Not Supported"); break;
    case ArchiveErrorCode::PasswordRequired:    title = tr("Password Required"); break;
    case ArchiveErrorCode::InsufficientDiskSpace: title = tr("Insufficient Disk Space"); break;
    case ArchiveErrorCode::PathTraversalBlocked: title = tr("Security: Path Traversal Blocked"); break;
    case ArchiveErrorCode::VolumeMissing:       title = tr("Volume Missing"); break;
    case ArchiveErrorCode::FileNotFound:        title = tr("File Not Found"); break;
    case ArchiveErrorCode::PermissionDenied:    title = tr("Permission Denied"); break;
    case ArchiveErrorCode::PathTooLong:         title = tr("Path Too Long"); break;
    case ArchiveErrorCode::EncodingError:       title = tr("Encoding Error"); break;
    case ArchiveErrorCode::PartialExtraction:   title = tr("Extraction Incomplete"); break;
    case ArchiveErrorCode::HardLinkUnsupported: title = tr("Hard Link Not Supported"); break;
    case ArchiveErrorCode::Unknown:             break;
    case ArchiveErrorCode::Cancelled:           break;
    }

    QMessageBox::critical(this, title, msg);

    if (!ArchiveManager::instance().backend()) {
        showPlaceholder(tr("Unable to open archive.\n%1").arg(msg));
    } else {
        // A failed attempt to open/reopen another archive leaves the previous
        // archive usable; its list is still valid.
        // 打开/重开另一个压缩包失败时，旧压缩包仍可用，其列表仍然有效。
        m_listReady = true;
    }
    updateNavLocation();
    updateNavigationState();
    updateActionsState();
    updateStatusSummary();
}

void MainWindow::onOperationFinished(bool success, const QString &msg)
{
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
    setBusyIndicatorVisible(false);

    const UiOp finishedOp = m_currentOp;
    m_currentOp = UiOp::None;

    if (!success) {
        m_oneClickExtractActive = false;
        m_oneClickCloseAfter = false;
        statusBar()->showMessage(msg.isEmpty() ? tr("Operation failed") : msg, 5000);
        updateNavLocation();
        updateNavigationState();
        updateActionsState();
        return;
    }

    statusBar()->showMessage(msg, 5000);

    if (finishedOp == UiOp::Extract && m_oneClickExtractActive) {
        const bool closeAfter = m_oneClickCloseAfter;
        m_oneClickExtractActive = false;
        m_oneClickCloseAfter = false;
        statusBar()->showMessage(
            tr("Extracted to %1").arg(m_lastExtractDest), 8000);

        if (closeAfter) {
            // Let the progress dialog disappear before closing the window.
            // 先关闭进度对话框，再关闭主窗口。
            QTimer::singleShot(650, this, [this]() { close(); });
            updateActionsState();
            return;
        }
    }

    if (finishedOp == UiOp::Extract && m_openDestAfterExtract
        && !m_lastExtractDest.isEmpty()) {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastExtractDest))) {
            MessageBoxUtils::showOpenFailure(this, m_lastExtractDest);
        }
    }

    if (finishedOp == UiOp::Test) {
        QMessageBox::information(
            this, tr("Test Archive"),
            tr("The archive test completed successfully. No errors were found."));
    }

    if (finishedOp == UiOp::Add || finishedOp == UiOp::Delete) {
        ListOptions opts;
        if (!m_currentPassword.isEmpty()) {
            opts.password = m_currentPassword;
        }
        m_currentOp = UiOp::List;
        ArchiveManager::instance().listContents(opts);
    }

    updateStatusSummary();
    updateActionsState();
}

void MainWindow::onEntryDoubleClicked(const QString &path)
{
    if (m_model->isFlatMode()) {
        if (path == QStringLiteral("..")) {
            const QString current = m_model->currentDirectory();
            const int slash = current.lastIndexOf(QLatin1Char('/'));
            navigateToDirectory(slash < 0 ? QString() : current.left(slash),
                                true);
            return;
        }

        const QModelIndex index = m_model->indexForPath(path);
        const ArchiveEntry *entry = m_model->entryAt(index);
        if (entry && entry->isDir) {
            navigateToDirectory(entry->path, true);
            return;
        }
    }

    const QModelIndex index = m_model->indexForPath(path);
    const ArchiveEntry *entry = m_model->entryAt(index);
    if (!entry || entry->isDir) {
        return;
    }

    const QStringList imagePaths =
        PreviewUtils::imageSiblings(m_model->sourceEntries(), entry->path);
    PreviewManager::instance().preview(
        ArchiveManager::instance().currentPath(),
        ArchiveManager::instance().currentFormat(),
        entry->path, entry->size, imagePaths, m_currentPassword);
}

void MainWindow::onContentViewModeChanged(const QString &mode)
{
    const QString normalized =
        (mode == QStringLiteral("list") || mode == QStringLiteral("tree"))
        ? mode : QStringLiteral("grid");
    if (normalized != contentViewMode()) {
        AppSettings::instance().setContentViewMode(normalized);
    }
    if (m_modernNavBar) {
        m_modernNavBar->setViewMode(normalized);
    }
    applyContentViewMode();
}

void MainWindow::onSelectionChanged()
{
    updateActionsState();
    updateSelectionDetails();
}

void MainWindow::onNavBack()
{
    navigateHistory(-1);
}

void MainWindow::onNavForward()
{
    navigateHistory(1);
}

void MainWindow::onNavUp()
{
    if (!m_model) {
        return;
    }
    if (m_model->isFlatMode()) {
        const QString current = m_model->currentDirectory();
        if (current.isEmpty()) {
            return;
        }
        const int slash = current.lastIndexOf(QLatin1Char('/'));
        navigateToDirectory(slash < 0 ? QString() : current.left(slash), true);
        return;
    }
    if (m_navCurrentDir.isEmpty()) {
        return;
    }
    const int slash = m_navCurrentDir.lastIndexOf(QLatin1Char('/'));
    navigateToDirectory(slash < 0 ? QString() : m_navCurrentDir.left(slash),
                        true);
}

void MainWindow::onFolderDirectoryActivated(const QString &path)
{
    navigateToDirectory(path, true);
}

void MainWindow::onTreeContextMenu(const QPoint &pos)
{
    auto *view = qobject_cast<QAbstractItemView *>(sender());
    if (!view) {
        view = currentContentView();
    }
    if (!view) {
        return;
    }

    const QModelIndex index = view->indexAt(pos);
    if (index.isValid() && view->selectionModel()) {
        view->selectionModel()->select(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        view->setCurrentIndex(index);
    }

    ArchiveBackend *backend = ArchiveManager::instance().backend();
    const bool busy = ArchiveManager::instance().isBusy();
    const QStringList selected = selectedEntryPaths();

    QMenu menu(this);
    QAction *extractSelected = menu.addAction(
        modernIcon(QStringLiteral("extract.png")), tr("Extract Selected..."));
    extractSelected->setEnabled(backend && !selected.isEmpty() && !busy);
    connect(extractSelected, &QAction::triggered, this, &MainWindow::onExtractSelected);

    QAction *oneClick = menu.addAction(
        modernIcon(QStringLiteral("oneclick.png")), tr("One-click Extract"));
    oneClick->setEnabled(backend && !busy);
    connect(oneClick, &QAction::triggered, this, &MainWindow::onOneClickExtract);

    menu.addSeparator();

    QAction *addFiles = menu.addAction(
        modernIcon(QStringLiteral("add.png")), tr("Add Files..."));
    addFiles->setEnabled(backend && ArchiveManager::instance().canModifyCurrentArchive()
                         && !busy);
    connect(addFiles, &QAction::triggered, this, &MainWindow::onAddFiles);

    QAction *deleteSelected = menu.addAction(
        modernIcon(QStringLiteral("delete.png")), tr("Delete Selected"));
    deleteSelected->setEnabled(backend
                               && ArchiveManager::instance().canModifyCurrentArchive()
                               && !selected.isEmpty() && !busy);
    connect(deleteSelected, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    menu.addSeparator();

    QAction *testArchive = menu.addAction(
        modernIcon(QStringLiteral("test.png")), tr("Test Archive"));
    testArchive->setEnabled(backend && !busy);
    connect(testArchive, &QAction::triggered, this, &MainWindow::onTestArchive);

    menu.exec(view->viewport()->mapToGlobal(pos));
    updateActionsState();
}

void MainWindow::onSearchChanged(const QString &text)
{
    m_searchText = text;
    m_proxyModel->setFilterWildcard(text.trimmed());
}

void MainWindow::onSidebarFileOpened(const QString &path)
{
    if (QFileInfo::exists(path)) {
        openFile(path);
    }
}

// Drag out: stage the selected entries as real files, then start a standard
// file URL drag.  The drop target (usually a file manager) performs the copy.
// 拖出：把选中条目暂存为真实文件，然后启动标准文件 URL 拖拽。
// 复制动作由放置目标（通常是文件管理器）执行。
namespace {

// Drag payload that reports who reads it: this is the only way to tell a
// rejected drop (the target never asks for data) from a broken URL list.
// 可报告谁读取了数据的拖拽负载：这是区分“放置被拒绝”（目标从未请求数据）
// 与“URL 列表损坏”的唯一方式。
class LoggingMimeData : public QMimeData {
public:
    explicit LoggingMimeData(const QStringList &entryPaths)
        : m_entryPaths(entryPaths) {}

protected:
    QVariant retrieveData(const QString &mimeType, QMetaType type) const override
    {
        if (m_reporting) {
            return QMimeData::retrieveData(mimeType, type);
        }
        m_reporting = true;
        const QVariant value = QMimeData::retrieveData(mimeType, type);
        m_reporting = false;
        LOG_INFO("DragOut", "drop target requested mime '%s' (entries=%lld, %s)",
                 mimeType.toUtf8().constData(),
                 static_cast<long long>(m_entryPaths.size()),
                 value.isValid() ? "served" : "empty");
        return value;
    }

private:
    QStringList m_entryPaths;
    mutable bool m_reporting = false;
};

} // namespace

void MainWindow::onEntriesDragOut(const QStringList &paths)
{
    auto &manager = ArchiveManager::instance();
    if (m_dragPrepareInProgress) {
        LOG_WARN("DragOut", "another drag preparation is still running, ignored");
        return;
    }
    if (paths.isEmpty()) {
        LOG_WARN("DragOut", "drag requested without selected entries");
        return;
    }
    if (!manager.backend()) {
        LOG_WARN("DragOut", "drag requested without an open archive");
        return;
    }
    if (manager.isBusy()) {
        LOG_WARN("DragOut", "drag requested while the archive manager is busy");
        return;
    }

    const QString archivePath = manager.currentPath();
    const ArchiveFormat format = manager.currentFormat();
    auto &dragOut = DragOutManager::instance();

    LOG_INFO("DragOut", "drag out: %lld entr%s from %s",
             static_cast<long long>(paths.size()),
             paths.size() == 1 ? "y" : "ies",
             archivePath.toUtf8().constData());

    DragOutPayload payload;
    if (dragOut.hasPreparedFor(archivePath, format, paths, m_currentPassword)) {
        LOG_INFO("DragOut", "reusing cached staging for this selection");
        payload = dragOut.takePreparedFor(archivePath, format, paths,
                                          m_currentPassword);
    } else {
        m_dragPrepareInProgress = true;

        const QList<ArchiveEntry> entries = m_model->sourceEntries();
        QString password = m_currentPassword;

        QApplication::setOverrideCursor(Qt::BusyCursor);
        setBusyIndicatorVisible(true, tr("Preparing files for drag..."));
        const QMetaObject::Connection progressConnection = connect(
            &dragOut, &DragOutManager::progressChanged, this,
            [this](int percent, int total, int, int) {
                if (total > 0) {
                    setBusyIndicatorVisible(
                        true, tr("Preparing files for drag... %1%").arg(percent));
                }
            });
        const QMetaObject::Connection messageConnection = connect(
            &dragOut, &DragOutManager::stageMessage, this,
            [this](const QString &message) {
                setBusyIndicatorVisible(true, message);
            });

        for (int attempt = 0;; ++attempt) {
            payload = dragOut.extractForDrag(archivePath, format, entries, paths,
                                             password);
            if (payload.success || payload.cancelled) {
                break;
            }
            const bool passwordIssue =
                payload.errorCode == ArchiveErrorCode::PasswordRequired
                || payload.errorCode == ArchiveErrorCode::WrongPassword;
            if (!passwordIssue) {
                break;
            }
            if (attempt >= 2) {
                payload.errorMessage = tr("Too many failed password attempts.");
                break;
            }

            QApplication::restoreOverrideCursor();
            PasswordDialog pw(this);
            const bool accepted = pw.exec() == QDialog::Accepted;
            QApplication::setOverrideCursor(Qt::BusyCursor);
            if (!accepted) {
                dragOut.cancel();
                payload.cancelled = true;
                payload.success = false;
                break;
            }
            password = pw.password();
            m_currentPassword = password;
            if (pw.rememberPassword()) {
                m_sessionPassword = password;
            }
        }

        disconnect(progressConnection);
        disconnect(messageConnection);
        QApplication::restoreOverrideCursor();
        setBusyIndicatorVisible(false);
        m_dragPrepareInProgress = false;
    }

    LOG_INFO("DragOut", "preparation finished: success=%d cancelled=%d staging=%s files=%lld",
             payload.success ? 1 : 0, payload.cancelled ? 1 : 0,
             payload.stagingRoot.toUtf8().constData(),
             static_cast<long long>(payload.localPaths.size()));

    if (!payload.success) {
        LOG_WARN("DragOut", "preparation failed: %s",
                 payload.errorMessage.toUtf8().constData());
        if (!payload.cancelled && !payload.errorMessage.isEmpty()) {
            QMessageBox::warning(this, tr("Drag Out"), payload.errorMessage);
        }
        updateActionsState();
        return;
    }

    const Qt::MouseButtons buttons = QGuiApplication::mouseButtons();
    LOG_INFO("DragOut", "mouse buttons at drag start: 0x%x",
             static_cast<unsigned>(buttons));
    if (!(buttons & Qt::LeftButton)) {
        LOG_WARN("DragOut", "left button already released, drag not started");
        dragOut.retire(payload);
        statusBar()->showMessage(
            tr("Files are ready. Drag the selection again to copy them out."),
            8000);
        updateActionsState();
        return;
    }

    QList<QUrl> urls;
    for (const QString &local : payload.localPaths) {
        urls << QUrl::fromLocalFile(local);
    }

    auto *mimeData = new LoggingMimeData(paths);
    mimeData->setUrls(urls);
    mimeData->setData(QStringLiteral("application/x-qarchiver-entries"),
                      paths.join(QLatin1Char('\n')).toUtf8());

    for (const QString &local : payload.localPaths) {
        if (!QFileInfo::exists(local)) {
            LOG_WARN("DragOut", "staged file is missing: %s",
                     local.toUtf8().constData());
        }
    }

    m_selfDropAttempted = false;
    auto *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    QElapsedTimer dragTimer;
    dragTimer.start();
    LOG_INFO("DragOut",
             "payload: hasUrls=%d formats=[%s] uri-list=%lld bytes",
             mimeData->hasUrls() ? 1 : 0,
             mimeData->formats().join(QStringLiteral(", ")).toUtf8().constData(),
             static_cast<long long>(
                 mimeData->data(QStringLiteral("text/uri-list")).size()));

    QObject::connect(drag, &QDrag::targetChanged, this,
                     [](QObject *target) {
                         LOG_INFO("DragOut", "drag target changed: %s",
                                  target ? target->metaObject()->className()
                                         : "none");
                     });
    LOG_INFO("DragOut", "starting QDrag with %lld urls, first=%s",
             static_cast<long long>(urls.size()),
             urls.isEmpty() ? "-" : urls.first().toLocalFile().toUtf8().constData());
    const bool skipNativeDrag =
        qEnvironmentVariableIsSet("QARCHIVER_DRAG_DEBUG_NO_EXEC");
    Qt::DropAction action = Qt::IgnoreAction;
    if (skipNativeDrag) {
        delete drag;
    } else {
        action = drag->exec(Qt::CopyAction, Qt::CopyAction);
    }
    const qint64 dragMillis = dragTimer.elapsed();
    LOG_INFO("DragOut", "QDrag finished: action=%d elapsed=%lldms selfDrop=%d",
             static_cast<int>(action), static_cast<long long>(dragMillis),
             m_selfDropAttempted ? 1 : 0);

    dragOut.retire(payload);
    updateActionsState();

    if (m_selfDropAttempted) {
        m_selfDropAttempted = false;
        QMessageBox::warning(
            this, tr("Drag Out"),
            tr("Cannot drop entries onto QArchiver itself.\n"
               "Please drop them onto a file manager, the desktop, or another "
               "application."));
    } else if (action == Qt::IgnoreAction) {
        statusBar()->showMessage(
            dragMillis < 150
                ? tr("Files are ready. Drag the selection again to copy them out.")
                : tr("The target application does not accept file drops."),
            6000);
    }
}

// Drag and drop

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Esc aborts a drag preparation that is still running (the mouse button is
    // held down at that point, so a dialog button cannot be used).
    // Esc 中止仍在进行的拖拽准备（此时鼠标按键被按住，无法使用对话框按钮）。
    if (event->key() == Qt::Key_Escape && m_dragPrepareInProgress) {
        DragOutManager::instance().cancel();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && !m_searchText.isEmpty()) {
        setSearchText(QString());
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat(
            QStringLiteral("application/x-qarchiver-entries"))) {
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
        return;
    }
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasFormat(
            QStringLiteral("application/x-qarchiver-entries"))) {
        m_selfDropAttempted = true;
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
        return;
    }
    if (!event->mimeData()->hasUrls()) {
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    QStringList localFiles;
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            localFiles << url.toLocalFile();
        }
    }
    if (localFiles.isEmpty()) {
        return;
    }

    if (localFiles.size() == 1 && isArchiveFile(localFiles.first())) {
        openFile(localFiles.first());
        event->acceptProposedAction();
        return;
    }

    if (ArchiveManager::instance().backend()) {
        addFilesToCurrentArchive(localFiles);
    } else {
        CompressDialog dlg(localFiles, this);
        if (dlg.exec() == QDialog::Accepted) {
            m_currentOp = UiOp::Compress;
            showProgressDialog(tr("Creating Archive..."));
            ArchiveManager::instance().compress(dlg.options(), dlg.selectedFormat());
        }
    }
    event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
    ArchiveManager::instance().closeArchive();
    PreviewManager::instance().cleanupOwn();
    DragOutManager::instance().cleanupOwn();
    event->accept();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange
        || event->type() == QEvent::StyleChange) {
        updateSearchIcon();
    }
    QMainWindow::changeEvent(event);
}
