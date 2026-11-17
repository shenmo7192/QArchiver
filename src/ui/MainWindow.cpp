#include "MainWindow.h"

#include "FileListModel.h"
#include "FilterProxyModel.h"
#include "FileListViewDelegate.h"
#include "PreviewManager.h"
#include "DragOutManager.h"
#include "widgets/ArchiveTreeView.h"
#include "widgets/SidebarWidget.h"
#include "dialogs/ExtractDialog.h"
#include "dialogs/CompressDialog.h"
#include "dialogs/PasswordDialog.h"
#include "utils/Logger.h"
#include "dialogs/ProgressDialog.h"
#include "dialogs/PreferencesDialog.h"

#include "core/ArchiveManager.h"
#include "core/ArchiveError.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveConstants.h"
#include "core/AppSettings.h"

#include "backends/ArchiveBackend.h"


#include <QAction>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QDropEvent>
#include <QGuiApplication>
#include <QMimeData>
#include <QUrl>
#include <QDesktopServices>
#include <QFileInfo>
#include <QCloseEvent>
#include <QPushButton>
#include <QListView>
#include <QTreeView>
#include <QAbstractItemView>
#include <QDialog>
#include <QProgressBar>
#include <QMenu>
#include <QDockWidget>
#include <QDir>
#include <QItemSelectionModel>
#include <QCoreApplication>

namespace {

QIcon resourceIcon(const QString &fileName)
{
    return QIcon(QStringLiteral(":/icons/") + fileName);
}

bool isArchiveFile(const QString &path)
{
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

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("QArchiver"));
    setWindowIcon(resourceIcon(QStringLiteral("QArchiver.png")));
    resize(1000, 650);

    m_model = new FileListModel(this);
    m_proxyModel = new FilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    setupActions();
    setupToolbar();
    setupCentralWidget();
    setupSidebar();
    applyContentViewMode();

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

    connect(m_treeView, &ArchiveTreeView::dragOutRequested,
            this, &MainWindow::onEntriesDragOut);

    PreviewManager::instance().cleanupStale();
    DragOutManager::instance().cleanupStale();
    updateActionsState();
}

MainWindow::~MainWindow()
{
    PreviewManager::instance().cleanupOwn();
    DragOutManager::instance().cleanupOwn();
}

void MainWindow::setupActions()
{
    m_openAct = new QAction(resourceIcon(QStringLiteral("Open.png")), tr("Open"), this);
    m_openAct->setShortcut(QKeySequence::Open);
    connect(m_openAct, &QAction::triggered, this, &MainWindow::onOpen);

    m_newAct = new QAction(resourceIcon(QStringLiteral("New.png")), tr("New Archive"), this);
    m_newAct->setShortcut(QKeySequence::New);
    connect(m_newAct, &QAction::triggered, this, &MainWindow::onNewArchive);

    // Toolbar Extract always extracts the whole archive
    m_extractAct = new QAction(resourceIcon(QStringLiteral("Extract.png")), tr("Extract"), this);
    connect(m_extractAct, &QAction::triggered, this, &MainWindow::onExtractAll);

    m_addAct = new QAction(resourceIcon(QStringLiteral("New.png")), tr("Add Files..."), this);
    connect(m_addAct, &QAction::triggered, this, &MainWindow::onAddFiles);

    m_deleteAct = new QAction(resourceIcon(QStringLiteral("Close.png")),
                              tr("Delete Selected"), this);
    connect(m_deleteAct, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    m_closeAct = new QAction(resourceIcon(QStringLiteral("Close.png")), tr("Close"), this);
    m_closeAct->setShortcut(QKeySequence::Close);
    connect(m_closeAct, &QAction::triggered, this, &MainWindow::onCloseArchive);
}

void MainWindow::setupToolbar()
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
    tb->addAction(m_addAct);
    tb->addAction(m_deleteAct);

    tb->addSeparator();
    tb->addAction(m_closeAct);

    QAction *prefsAct = new QAction(resourceIcon(QStringLiteral("Preferences.png")),
                                    tr("Preferences"), this);
    connect(prefsAct, &QAction::triggered, this, &MainWindow::onPreferences);
    tb->addAction(prefsAct);

    QAction *aboutAct = new QAction(resourceIcon(QStringLiteral("About.png")),
                                    tr("About"), this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
    tb->addAction(aboutAct);

    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    QAction *quitAct = new QAction(resourceIcon(QStringLiteral("Quit.png")), tr("Quit"), this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    tb->addAction(quitAct);

    // Ensure icon-only mode still has useful hover text.
    for (QAction *action : tb->actions()) {
        if (!action->isSeparator() && action->toolTip().isEmpty()) {
            action->setToolTip(action->text());
        }
    }
    applyToolButtonStyle();
}

void MainWindow::setupCentralWidget()
{
    m_centralStack = new QStackedWidget(this);

    // Placeholder page (shown when no archive is open)
    auto *placeholder = new QWidget(m_centralStack);
    auto *placeholderLayout = new QVBoxLayout(placeholder);
    placeholderLayout->addStretch(1);
    m_placeholderLabel = new QLabel(
        tr("No archive open.\nUse the Open button or drag a file here."),
        placeholder);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    m_placeholderLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 14px;"));
    placeholderLayout->addWidget(m_placeholderLabel);
    auto *openBtn = new QPushButton(resourceIcon(QStringLiteral("Open.png")),
                                    tr("Choose Archive..."), placeholder);
    openBtn->setIconSize(QSize(20, 20));
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpen);
    placeholderLayout->addWidget(openBtn, 0, Qt::AlignHCenter);
    placeholderLayout->addStretch(1);

    // Archive page: search field + tree
    auto *archivePage = new QWidget(m_centralStack);
    auto *archiveLayout = new QVBoxLayout(archivePage);
    archiveLayout->setContentsMargins(0, 0, 0, 0);
    archiveLayout->setSpacing(2);

    m_searchEdit = new QLineEdit(archivePage);
    m_searchEdit->setPlaceholderText(tr("Filter by name or path (wildcards allowed)..."));
    m_searchEdit->setClearButtonEnabled(true);
    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &MainWindow::onSearchChanged);
    archiveLayout->addWidget(m_searchEdit);

    m_treeView = new ArchiveTreeView(archivePage);
    m_treeView->setModel(m_proxyModel);
    m_treeView->setItemDelegate(new FileListViewDelegate(m_treeView));
    connect(m_treeView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() { updateActionsState(); });
    auto *header = m_treeView->header();
    header->setStretchLastSection(false);
    header->setSectionsClickable(true);
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setMinimumSectionSize(60);
    header->resizeSection(FileListModel::NameColumn, 360);
    header->resizeSection(FileListModel::SizeColumn, 100);
    header->resizeSection(FileListModel::PackedSizeColumn, 100);
    header->resizeSection(FileListModel::ModifiedColumn, 150);
    header->resizeSection(FileListModel::EncryptedColumn, 90);
    m_treeView->setIconSize(QSize(20, 20));
    m_treeView->sortByColumn(FileListModel::NameColumn, Qt::AscendingOrder);
    m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeView, &QTreeView::customContextMenuRequested,
            this, &MainWindow::onTreeContextMenu);
    connect(m_treeView, &ArchiveTreeView::entryDoubleClicked,
            this, &MainWindow::onEntryDoubleClicked);
    archiveLayout->addWidget(m_treeView, 1);

    m_centralStack->addWidget(placeholder);   // index 0
    m_centralStack->addWidget(archivePage);   // index 1
    m_centralStack->setCurrentIndex(0);
    setCentralWidget(m_centralStack);

    // Busy indicator on the right side of the status bar
    m_busyLabel = new QLabel(this);
    m_busyBar = new QProgressBar(this);
    m_busyBar->setRange(0, 0);
    m_busyBar->setFixedWidth(120);
    m_busyBar->setTextVisible(false);
    m_busyWidget = new QWidget(this);
    auto *busyLayout = new QHBoxLayout(m_busyWidget);
    busyLayout->setContentsMargins(0, 0, 0, 0);
    busyLayout->setSpacing(6);
    busyLayout->addWidget(m_busyLabel);
    busyLayout->addWidget(m_busyBar);
    m_busyWidget->hide();
    statusBar()->addPermanentWidget(m_busyWidget);

    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupSidebar()
{
    m_sidebarDock = new QDockWidget(tr("Recent Files"), this);
    m_sidebarDock->setObjectName(QStringLiteral("SidebarDock"));
    // Title bar of the dock is the only heading of the sidebar.
    m_sidebarDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto *sidebar = new SidebarWidget(m_sidebarDock);
    m_sidebarDock->setWidget(sidebar);
    addDockWidget(Qt::LeftDockWidgetArea, m_sidebarDock);
    connect(sidebar, &SidebarWidget::fileOpened,
            this, &MainWindow::onSidebarFileOpened);

    applySidebarVisibility();
}

void MainWindow::applyContentViewMode()
{
    const bool flat = AppSettings::instance().contentViewMode() == QStringLiteral("list");
    m_model->setFlatMode(flat);
    m_treeView->setFlatMode(flat);

    // Re-apply the current header sort to whichever model now owns sorting.
    const int sortSection = m_treeView->header()->sortIndicatorSection();
    const Qt::SortOrder sortOrder = m_treeView->header()->sortIndicatorOrder();
    m_treeView->sortByColumn(sortSection >= 0 ? sortSection : FileListModel::NameColumn,
                             sortSection >= 0 ? sortOrder : Qt::AscendingOrder);

    if (!flat) {
        m_treeView->expandToDepth(0);
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

void MainWindow::updateActionsState()
{
    ArchiveBackend *backend = ArchiveManager::instance().backend();
    const bool hasArchive = (backend != nullptr);
    const bool canModify = hasArchive && backend->supportsAppend();
    const bool hasSelection = !selectedEntryPaths().isEmpty();

    m_extractAct->setEnabled(hasArchive);
    m_addAct->setEnabled(canModify);
    m_deleteAct->setEnabled(canModify && hasSelection);
    m_closeAct->setEnabled(hasArchive);
}

void MainWindow::showPlaceholder(const QString &message)
{
    m_placeholderLabel->setText(message);
    m_centralStack->setCurrentIndex(0);
}

QStringList MainWindow::chooseSourceItems(const QString &title)
{
    MixedSelectionFileDialog fileDialog(this, title);
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setOption(QFileDialog::DontUseNativeDialog, true);

    if (auto *listView = fileDialog.findChild<QListView *>(QStringLiteral("listView"))) {
        listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    if (auto *treeView = fileDialog.findChild<QTreeView *>(QStringLiteral("treeView"))) {
        treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    if (fileDialog.exec() != QDialog::Accepted) {
        return {};
    }
    return fileDialog.selectedFiles();
}

void MainWindow::openFile(const QString &path)
{
    m_currentPassword = m_sessionPassword;
    m_model->setEntries({});
    m_searchEdit->clear();
    ArchiveManager::instance().openArchive(path);
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
    showProgressDialog(tr("Extracting..."));
    ArchiveManager::instance().extract(options);
}

void MainWindow::addFilesToCurrentArchive(const QStringList &files)
{
    ArchiveBackend *backend = ArchiveManager::instance().backend();
    if (!backend || files.isEmpty()) {
        return;
    }
    if (!backend->supportsAppend()) {
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
        this, tr("Open Archive"), QString(),
        tr("Archives (*.tar *.tar.gz *.tgz *.tar.xz *.txz *.tar.bz2 *.tbz2 *.zip *.7z *.rar *.gz *.bz2 *.xz *.001);;All Files (*)"));
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

void MainWindow::onAddFiles()
{
    if (!ArchiveManager::instance().backend()) {
        return;
    }
    const QStringList items = chooseSourceItems(tr("Select Files to Add"));
    addFilesToCurrentArchive(items);
}

void MainWindow::onDeleteSelected()
{
    ArchiveBackend *backend = ArchiveManager::instance().backend();
    const QStringList paths = selectedEntryPaths();
    if (!backend || paths.isEmpty()) {
        return;
    }
    if (!backend->supportsAppend()) {
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
    PreferencesDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        applyContentViewMode();
        applyToolButtonStyle();
        applySidebarVisibility();
    }
}

void MainWindow::onCloseArchive()
{
    ArchiveManager::instance().closeArchive();
    // Staged drag data belongs to the archive that is being closed.
    DragOutManager::instance().clearCache();
    m_model->setEntries({});
    m_searchEdit->clear();

    m_currentPassword.clear();
    if (!AppSettings::instance().rememberPasswords()) {
        m_sessionPassword.clear();
    }

    setBusyIndicatorVisible(false);
    setWindowTitle(tr("QArchiver"));
    showPlaceholder(tr("No archive open.\nUse the Open button or drag a file here."));
    statusBar()->showMessage(tr("Archive closed"), 3000);
    updateActionsState();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About QArchiver"),
        tr("<h3>QArchiver (小Q解压)</h3>"
           "<p>A lightweight Qt-based archive manager for Linux.</p>"
           "<p>Supports: tar, tar.gz, tar.xz, tar.bz2, zip, 7z, rar, gz, bz2, xz</p>"
           "<p>Version: %1</p>")
            .arg(QCoreApplication::applicationVersion()));
}

void MainWindow::onArchiveOpening(const QString &filePath)
{
    setBusyIndicatorVisible(true,
        tr("Detecting format: %1").arg(QFileInfo(filePath).fileName()));
    statusBar()->showMessage(tr("Detecting archive format..."));
}

void MainWindow::onArchiveOpened(const QString &filePath)
{
    // A new archive means new content: drop staging prepared for the previous
    // one.  (The staging directories themselves are removed on exit.)
    DragOutManager::instance().clearCache();
    setWindowTitle(tr("QArchiver - %1").arg(QFileInfo(filePath).fileName()));
    m_centralStack->setCurrentIndex(1);
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
    m_model->setEntries(entries);
    if (!m_model->isFlatMode()) {
        m_treeView->expandToDepth(0);
    }
    setBusyIndicatorVisible(false);
    statusBar()->showMessage(tr("%1 entries").arg(entries.size()), 3000);
    updateActionsState();
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
    if (dlg.rememberPassword() || AppSettings::instance().rememberPasswords()) {
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
    setBusyIndicatorVisible(false);

    if (code == ArchiveErrorCode::WrongPassword) {
        m_currentPassword.clear();
    }

    if (code == ArchiveErrorCode::Cancelled) {
        statusBar()->showMessage(msg.isEmpty() ? tr("Cancelled") : msg, 5000);
        updateActionsState();
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
    case ArchiveErrorCode::Unknown:             break;
    case ArchiveErrorCode::Cancelled:           break;
    }

    QMessageBox::critical(this, title, msg);

    if (!ArchiveManager::instance().backend()) {
        showPlaceholder(tr("Unable to open archive.\n%1").arg(msg));
    }
    updateActionsState();
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
        statusBar()->showMessage(msg.isEmpty() ? tr("Operation failed") : msg, 5000);
        updateActionsState();
        return;
    }

    statusBar()->showMessage(msg, 5000);

    if (finishedOp == UiOp::Extract && m_openDestAfterExtract
        && !m_lastExtractDest.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastExtractDest));
    }

    if (finishedOp == UiOp::Add || finishedOp == UiOp::Delete) {
        ListOptions opts;
        if (!m_currentPassword.isEmpty()) {
            opts.password = m_currentPassword;
        }
        m_currentOp = UiOp::List;
        ArchiveManager::instance().listContents(opts);
    }

    updateActionsState();
}

void MainWindow::onEntryDoubleClicked(const QString &path)
{
    if (m_model->isFlatMode()) {
        if (path == QStringLiteral("..")) {
            if (m_model->goUp()) {
                m_searchEdit->clear();
                m_treeView->scrollToTop();
                const QString dir = m_model->currentDirectory();
                statusBar()->showMessage(dir.isEmpty() ? QStringLiteral("/")
                                                       : QLatin1Char('/') + dir, 2000);
            }
            return;
        }

        const QModelIndex index = m_model->indexForPath(path);
        const ArchiveEntry *entry = m_model->entryAt(index);
        if (entry && entry->isDir) {
            if (m_model->enterDirectory(entry->path)) {
                m_searchEdit->clear();
                m_treeView->scrollToTop();
                statusBar()->showMessage(QLatin1Char('/') + m_model->currentDirectory(), 2000);
            }
            return;
        }
    }

    const QModelIndex index = m_model->indexForPath(path);
    const ArchiveEntry *entry = m_model->entryAt(index);
    if (!entry || entry->isDir) {
        return;
    }

    PreviewManager::instance().preview(
        ArchiveManager::instance().currentPath(),
        ArchiveManager::instance().currentFormat(),
        entry->path, entry->size, m_currentPassword);
}

void MainWindow::onTreeContextMenu(const QPoint &pos)
{
    const QModelIndex index = m_treeView->indexAt(pos);
    if (index.isValid()) {
        m_treeView->selectionModel()->select(
            index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        m_treeView->setCurrentIndex(index);
    }

    ArchiveBackend *backend = ArchiveManager::instance().backend();
    const QStringList selected = selectedEntryPaths();

    QMenu menu(this);
    QAction *extractSelected = menu.addAction(
        resourceIcon(QStringLiteral("Extract.png")), tr("Extract Selected..."));
    extractSelected->setEnabled(backend && !selected.isEmpty());
    connect(extractSelected, &QAction::triggered, this, &MainWindow::onExtractSelected);

    menu.addSeparator();

    QAction *addFiles = menu.addAction(
        resourceIcon(QStringLiteral("New.png")), tr("Add Files..."));
    addFiles->setEnabled(backend && backend->supportsAppend());
    connect(addFiles, &QAction::triggered, this, &MainWindow::onAddFiles);

    QAction *deleteSelected = menu.addAction(
        resourceIcon(QStringLiteral("Close.png")), tr("Delete Selected"));
    deleteSelected->setEnabled(backend && backend->supportsAppend() && !selected.isEmpty());
    connect(deleteSelected, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    menu.exec(m_treeView->viewport()->mapToGlobal(pos));
    updateActionsState();
}

void MainWindow::onSearchChanged(const QString &text)
{
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
namespace {

// Drag payload that reports who reads it: this is the only way to tell a
// rejected drop (the target never asks for data) from a broken URL list.
class LoggingMimeData : public QMimeData {
public:
    explicit LoggingMimeData(const QStringList &entryPaths)
        : m_entryPaths(entryPaths) {}

protected:
    QVariant retrieveData(const QString &mimeType, QMetaType type) const override
    {
        // QMimeData::urls() is implemented on top of data(), which dispatches
        // back into retrieveData().  Calling urls()/hasUrls()/data() from this
        // override therefore recurses until the stack overflows - that was the
        // SIGSEGV right after the drag started.  Report only the payload we set
        // ourselves and guard re-entrancy as well.
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
        // A previous preparation finished (possibly after the user released
        // the button): reuse it so this drag starts with no delay at all.
        payload = dragOut.takePreparedFor(archivePath, format, paths,
                                          m_currentPassword);
    } else {
        // The extraction has to happen while the mouse button is still down, so
        // no dialog window is shown here: that would break the drag gesture and
        // the drop target would never receive any URLs.  A busy cursor plus the
        // status bar keeps the gesture (and Esc) working.
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
                // Three prompts are enough; stop instead of looping forever.
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
            if (pw.rememberPassword()
                || AppSettings::instance().rememberPasswords()) {
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

    // A file drag can only start while the button is still held down.  If the
    // user already released it the staging is cached, so the next drag of the
    // same selection starts instantly - say so instead of doing nothing.
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

    // setUrls() provides the standard text/uri-list.  The custom MIME type only
    // marks the drag as ours so a drop back onto QArchiver is rejected instead
    // of being treated as "add these files".
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
    // The QDrag has to live on the heap (Qt deletes it later via
    // deleteLater()).  QDrag::exec() returns as soon as the button is
    // released, but the drop target fetches the URL list from this very
    // QMimeData *afterwards* (the X11 selection request arrives later), so a
    // stack-allocated QDrag would destroy the payload together with the drag:
    // the target then receives an empty uri-list and silently copies nothing.
    auto *drag = new QDrag(this);
    drag->setMimeData(mimeData);
    QElapsedTimer dragTimer;
    dragTimer.start();
    // Self check: what the drop target will actually read from the payload.
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
    // Debug aid: QARCHIVER_DRAG_DEBUG_NO_EXEC=1 stages the selection and logs
    // the URLs without entering the (native, blocking) drag loop, so the
    // staging can be inspected when a real drag is not available.
    const bool skipNativeDrag =
        qEnvironmentVariableIsSet("QARCHIVER_DRAG_DEBUG_NO_EXEC");
    Qt::DropAction action = Qt::IgnoreAction;
    if (skipNativeDrag) {
        // Debug path: exec() is never entered, so nothing will delete the drag.
        delete drag;
    } else {
        action = drag->exec(Qt::CopyAction, Qt::CopyAction);
    }
    const qint64 dragMillis = dragTimer.elapsed();
    LOG_INFO("DragOut", "QDrag finished: action=%d elapsed=%lldms selfDrop=%d",
             static_cast<int>(action), static_cast<long long>(dragMillis),
             m_selfDropAttempted ? 1 : 0);

    // The drop target may still be copying, so the staging stays until exit.
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
        // A drag that is cancelled instantly usually means the platform refused
        // to start it (e.g. the button state changed while preparing); the
        // staged files are cached, so asking for a second attempt is cheap.
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
    if (event->key() == Qt::Key_Escape && m_dragPrepareInProgress) {
        DragOutManager::instance().cancel();
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    // Entries dragged out of our own view must never be treated as files to
    // add: the URLs point at drag staging, not at real user files.
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
        // Dropped back onto QArchiver itself: remember it so the warning can be
        // shown once QDrag::exec() returns, and do nothing here.
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

// Helpers

void MainWindow::setBusyIndicatorVisible(bool visible, const QString &text)
{
    if (!m_busyWidget) {
        return;
    }
    m_busyLabel->setText(text);
    m_busyWidget->setVisible(visible);
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
}

QStringList MainWindow::selectedEntryPaths() const
{
    return m_treeView ? m_treeView->selectedEntryPaths() : QStringList();
}
