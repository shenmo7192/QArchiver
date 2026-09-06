#include "MainWindow.h"

#include "FileListModel.h"
#include "FilterProxyModel.h"
#include "FileListViewDelegate.h"
#include "PreviewManager.h"
#include "widgets/ArchiveTreeView.h"
#include "dialogs/ExtractDialog.h"
#include "dialogs/CompressDialog.h"
#include "dialogs/PasswordDialog.h"
#include "dialogs/ProgressDialog.h"
#include "dialogs/PreferencesDialog.h"
#include "dialogs/ArchivePropertiesDialog.h"

#include "core/ArchiveManager.h"
#include "core/ArchiveError.h"
#include "core/ArchiveOptions.h"
#include "core/ArchiveConstants.h"

#include "backends/ArchiveBackend.h"

#include "utils/Logger.h"

#include <QAction>
#include <QToolBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QHeaderView>
#include <QLabel>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
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

namespace {

// Icons come from src/resources/icons (embedded via resources.qrc).
QIcon resourceIcon(const QString &fileName)
{
    return QIcon(QStringLiteral(":/icons/") + fileName);
}

bool isArchiveFile(const QString &path)
{
    const QString lower = path.toLower();
    return lower.endsWith(QStringLiteral(".tar"))
        || lower.endsWith(QStringLiteral(".tar.gz"))
        || lower.endsWith(QStringLiteral(".tgz"))
        || lower.endsWith(QStringLiteral(".tar.xz"))
        || lower.endsWith(QStringLiteral(".txz"))
        || lower.endsWith(QStringLiteral(".tar.bz2"))
        || lower.endsWith(QStringLiteral(".tbz2"))
        || lower.endsWith(QStringLiteral(".zip"))
        || lower.endsWith(QStringLiteral(".7z"))
        || lower.endsWith(QStringLiteral(".rar"))
        || lower.endsWith(QStringLiteral(".gz"))
        || lower.endsWith(QStringLiteral(".bz2"))
        || lower.endsWith(QStringLiteral(".xz"))
        || lower.endsWith(QStringLiteral(".001"));
}

// QFileDialog that allows selecting files and directories together.
// Pressing Open accepts the current selection (files and/or directories),
// while double-clicking a directory still enters it.
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
    resize(900, 600);

    m_model = new FileListModel(this);
    m_proxyModel = new FilterProxyModel(this);
    m_proxyModel->setSourceModel(m_model);

    setupActions();
    setupToolbar();
    setupCentralWidget();

    // Connect ArchiveManager signals
    auto &manager = ArchiveManager::instance();
    connect(&manager, &ArchiveManager::archiveOpened, this, &MainWindow::onArchiveOpened);
    connect(&manager, &ArchiveManager::fileListReady, this, &MainWindow::onFileListReady);
    connect(&manager, &ArchiveManager::progressChanged, this, &MainWindow::onProgressChanged);
    connect(&manager, &ArchiveManager::outputReady, this, &MainWindow::onOutputReady);
    connect(&manager, &ArchiveManager::passwordRequested, this, &MainWindow::onPasswordRequested);
    connect(&manager, &ArchiveManager::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(&manager, &ArchiveManager::operationFinished, this, &MainWindow::onOperationFinished);

    // Cleanup stale previews from previous crashed instances
    PreviewManager::instance().cleanupStale();

    updateActionsState();
}

MainWindow::~MainWindow()
{
    PreviewManager::instance().cleanupOwn();
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

    m_propertiesAct = new QAction(resourceIcon(QStringLiteral("Properties.png")),
                                  tr("Properties"), this);
    connect(m_propertiesAct, &QAction::triggered, this, &MainWindow::onProperties);

    m_closeAct = new QAction(resourceIcon(QStringLiteral("Close.png")), tr("Close"), this);
    m_closeAct->setShortcut(QKeySequence::Close);
    connect(m_closeAct, &QAction::triggered, this, &MainWindow::onCloseArchive);
}

void MainWindow::setupToolbar()
{
    auto *tb = addToolBar(tr("Main"));
    tb->setObjectName(QStringLiteral("MainToolBar"));
    tb->setMovable(false);
    tb->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    tb->setIconSize(QSize(32, 32));

    tb->addAction(m_openAct);
    tb->addAction(m_newAct);

    tb->addSeparator();
    tb->addAction(m_extractAct);

    tb->addSeparator();
    tb->addAction(m_propertiesAct);
    tb->addAction(m_closeAct);

    QAction *prefsAct = new QAction(resourceIcon(QStringLiteral("Preferences.png")),
                                    tr("Preferences"), this);
    connect(prefsAct, &QAction::triggered, this, &MainWindow::onPreferences);
    tb->addAction(prefsAct);

    QAction *logAct = new QAction(resourceIcon(QStringLiteral("Log.png")),
                                  tr("Log"), this);
    connect(logAct, &QAction::triggered, this, &MainWindow::onViewLog);
    tb->addAction(logAct);

    QAction *aboutAct = new QAction(resourceIcon(QStringLiteral("About.png")),
                                    tr("About"), this);
    connect(aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
    tb->addAction(aboutAct);

    // Expanding spacer keeps the preceding buttons left-aligned and pushes
    // the Quit button to the far right of the toolbar.
    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);

    QAction *quitAct = new QAction(resourceIcon(QStringLiteral("Quit.png")), tr("Quit"), this);
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, qApp, &QApplication::quit);
    tb->addAction(quitAct);
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
    auto *openBtn = new QPushButton(tr("Choose Archive..."), placeholder);
    connect(openBtn, &QPushButton::clicked, this, &MainWindow::onOpen);
    placeholderLayout->addWidget(openBtn, 0, Qt::AlignHCenter);
    placeholderLayout->addStretch(1);

    // Archive tree page
    m_treeView = new ArchiveTreeView(m_centralStack);
    m_treeView->setModel(m_proxyModel);
    m_treeView->setItemDelegate(new FileListViewDelegate(m_treeView));
    m_treeView->header()->setStretchLastSection(false);
    m_treeView->header()->setSectionResizeMode(FileListModel::NameColumn, QHeaderView::Stretch);
    for (int col = FileListModel::SizeColumn; col < FileListModel::ColumnCount; ++col) {
        m_treeView->header()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    }

    connect(m_treeView, &ArchiveTreeView::entryDoubleClicked, this, &MainWindow::onEntryDoubleClicked);

    m_centralStack->addWidget(placeholder);   // index 0
    m_centralStack->addWidget(m_treeView);    // index 1
    m_centralStack->setCurrentIndex(0);

    setCentralWidget(m_centralStack);

    // Busy indicator on the right side of the status bar, shown while an
    // archive is being read.
    m_busyLabel = new QLabel(this);
    m_busyBar = new QProgressBar(this);
    m_busyBar->setRange(0, 0);  // indeterminate/busy mode
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

void MainWindow::updateActionsState()
{
    const bool hasArchive = (ArchiveManager::instance().backend() != nullptr);

    m_extractAct->setEnabled(hasArchive);
    m_propertiesAct->setEnabled(hasArchive);
    m_closeAct->setEnabled(hasArchive);
}

void MainWindow::showPlaceholder(const QString &message)
{
    m_placeholderLabel->setText(message);
    m_centralStack->setCurrentIndex(0);
}

void MainWindow::openFile(const QString &path)
{
    m_currentPassword.clear();
    m_model->setEntries({});
    ArchiveManager::instance().openArchive(path);
}

void MainWindow::startExtract(const ExtractOptions &opts)
{
    ExtractOptions options = opts;
    if (!m_currentPassword.isEmpty()) {
        options.password = m_currentPassword;
    }
    m_currentOp = UiOp::Extract;
    showProgressDialog(tr("Extracting..."));
    ArchiveManager::instance().extract(options);
}

// Slots

void MainWindow::onOpen()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Archive"), QString(),
        tr("Archives (*.tar *.tar.gz *.tgz *.tar.xz *.txz *.tar.bz2 *.tbz2 *.zip *.7z *.rar *.gz *.bz2 *.xz *.001);;All Files (*)"));
    if (!path.isEmpty()) {
        openFile(path);
    }
}

void MainWindow::onNewArchive()
{
    MixedSelectionFileDialog fileDialog(this, tr("Select Files to Compress"));
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    fileDialog.setOption(QFileDialog::DontUseNativeDialog, true);

    // Allow files and directories to be selected together. Double-clicking a
    // directory still enters it; Open accepts the whole selection.
    if (auto *listView = fileDialog.findChild<QListView *>(QStringLiteral("listView"))) {
        listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }
    if (auto *treeView = fileDialog.findChild<QTreeView *>(QStringLiteral("treeView"))) {
        treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    }

    if (fileDialog.exec() != QDialog::Accepted) {
        return;
    }

    const QStringList items = fileDialog.selectedFiles();
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

// Toolbar Extract: always extracts the whole archive
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

void MainWindow::onPreferences()
{
    PreferencesDialog dlg(this);
    dlg.exec();
}

void MainWindow::onProperties()
{
    ArchiveBackend *backend = ArchiveManager::instance().backend();
    if (!backend) {
        return;
    }
    ArchivePropertiesDialog dlg(backend, this);
    dlg.exec();
}

void MainWindow::onCloseArchive()
{
    ArchiveManager::instance().closeArchive();
    m_model->setEntries({});
    m_currentPassword.clear();
    setBusyIndicatorVisible(false);
    setWindowTitle(tr("QArchiver"));
    showPlaceholder(tr("No archive open.\nUse the Open button or drag a file here."));
    statusBar()->showMessage(tr("Archive closed"), 3000);
    updateActionsState();
}

void MainWindow::onViewLog()
{
    const QString logPath = Logger::instance().logFilePath();
    if (logPath.isEmpty() || !QFileInfo::exists(logPath)) {
        QMessageBox::information(this, tr("Log"),
                                 tr("No log file has been created yet."));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About QArchiver"),
        tr("<h3>QArchiver (小Q解压)</h3>"
           "<p>A lightweight Qt-based archive manager for Linux.</p>"
           "<p>Supports: tar, tar.gz, tar.xz, tar.bz2, zip, 7z, rar, gz, bz2, xz</p>"
           "<p>Version: 0.0.1</p>"));
}

void MainWindow::onArchiveOpened(const QString &filePath)
{
    setWindowTitle(tr("QArchiver - %1").arg(QFileInfo(filePath).fileName()));
    m_centralStack->setCurrentIndex(1);
    statusBar()->showMessage(tr("Opened: %1").arg(filePath), 3000);
    setBusyIndicatorVisible(true, tr("Reading archive %1").arg(QFileInfo(filePath).fileName()));

    // Auto-list contents (pass the session password if already known)
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
    PasswordDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) {
        m_currentPassword = dlg.password();
        // Re-run the pending operation with the password
        ArchiveManager::instance().retryWithPassword(m_currentPassword);
    }
}

void MainWindow::onErrorOccurred(ArchiveErrorCode code, const QString &msg)
{
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
    setBusyIndicatorVisible(false);

    QString title = tr("Error");
    switch (code) {
    case ArchiveErrorCode::UnsupportedFormat:
        title = tr("Unsupported Format");
        break;
    case ArchiveErrorCode::WrongPassword:
        title = tr("Wrong Password");
        break;
    case ArchiveErrorCode::CorruptedArchive:
        title = tr("Corrupted Archive");
        break;
    case ArchiveErrorCode::ToolNotFound:
        title = tr("Required Tool Not Found");
        break;
    case ArchiveErrorCode::OperationNotSupported:
        title = tr("Operation Not Supported");
        break;
    case ArchiveErrorCode::PasswordRequired:
        title = tr("Password Required");
        break;
    case ArchiveErrorCode::InsufficientDiskSpace:
        title = tr("Insufficient Disk Space");
        break;
    case ArchiveErrorCode::PathTraversalBlocked:
        title = tr("Security: Path Traversal Blocked");
        break;
    case ArchiveErrorCode::VolumeMissing:
        title = tr("Volume Missing");
        break;
    case ArchiveErrorCode::FileNotFound:
        title = tr("File Not Found");
        break;
    case ArchiveErrorCode::PermissionDenied:
        title = tr("Permission Denied");
        break;
    case ArchiveErrorCode::PathTooLong:
        title = tr("Path Too Long");
        break;
    case ArchiveErrorCode::EncodingError:
        title = tr("Encoding Error");
        break;
    case ArchiveErrorCode::Cancelled:
        title = tr("Cancelled");
        break;
    case ArchiveErrorCode::Unknown:
        break;
    }

    QMessageBox::critical(this, title, msg);

    // If opening failed and there is no backend, return to placeholder state
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

    if (success) {
        statusBar()->showMessage(msg, 5000);
        // Refresh the listing after files were added via drag-and-drop
        if (m_currentOp == UiOp::Add) {
            ListOptions opts;
            if (!m_currentPassword.isEmpty()) {
                opts.password = m_currentPassword;
            }
            ArchiveManager::instance().listContents(opts);
        }
    } else {
        statusBar()->showMessage(msg.isEmpty() ? tr("Operation failed") : msg, 5000);
    }
    m_currentOp = UiOp::None;
}

void MainWindow::onEntryDoubleClicked(const QString &path)
{
    ArchiveBackend *backend = ArchiveManager::instance().backend();
    if (!backend) {
        return;
    }

    for (int i = 0; i < m_model->rowCount(); ++i) {
        const ArchiveEntry *e = m_model->entryAt(i);
        if (e && e->path == path) {
            if (e->isDir) {
                return; // Directories are not previewed
            }
            PreviewManager::instance().preview(backend, path, e->size);
            return;
        }
    }
}

// Drag and drop

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
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

    // Dragging a single archive onto the window opens it
    if (localFiles.size() == 1 && isArchiveFile(localFiles.first())) {
        openFile(localFiles.first());
        event->acceptProposedAction();
        return;
    }

    // Otherwise create a new archive from the dropped files
    CompressDialog dlg(localFiles, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_currentOp = UiOp::Compress;
        showProgressDialog(tr("Creating Archive..."));
        ArchiveManager::instance().compress(dlg.options(), dlg.selectedFormat());
    }
    event->acceptProposedAction();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_progressDialog) {
        m_progressDialog->close();
        m_progressDialog = nullptr;
    }
    PreviewManager::instance().cleanupOwn();
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
    connect(dlg, &ProgressDialog::cancelled, this, [dlg, this]() {
        // The blocking backend processes cannot be interrupted safely;
        // the dialog is dismissed and the result is reported when finished.
        dlg->close();
        statusBar()->showMessage(tr("Operation continues in the background..."), 5000);
    });
    m_progressDialog = dlg;
    dlg->show();
}

QStringList MainWindow::selectedEntryPaths() const
{
    return m_treeView ? m_treeView->selectedEntryPaths() : QStringList();
}
