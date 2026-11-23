// QArchiver / 小Q解压
// MainWindow layout: modern and classic chrome, toolbars, sidebars, content
// views, status bar and view-mode switches.
// MainWindow 布局：现代与经典界面外观、工具栏、侧边栏、内容视图、
// 状态栏与视图模式切换。

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
    auto *openBtn = new QPushButton(Icons::aero(QStringLiteral("Open.png")),
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

void MainWindow::updateSearchIcon()
{
    if (!m_searchAction) {
        return;
    }
    m_searchAction->setIcon(Icons::themed(QStringLiteral("search")));
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
