// QArchiver / 小Q解压
// MainWindow navigation: directory history, location bar, selection details,
// search and the folder pane.
// MainWindow 导航：目录历史、位置栏、选中详情、搜索与文件夹面板。

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

QString MainWindow::contentViewMode() const
{
    // The classic layout has no view-mode switcher (no view buttons in the
    // toolbar, no view menu), so it always uses the tree view. The stored
    // grid/list choice still applies when the modern layout is active.
    // 经典布局没有视图模式切换入口（工具栏无视图按钮、也没有视图菜单），
    // 因此始终使用树视图；存储的网格/列表选择只在现代布局下生效。
    if (!m_modernUi) {
        return QStringLiteral("tree");
    }
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
                .arg(format, MainWindowHelpers::formatFileSize(unpacked))
                .arg(ratio, 0, 'f', 1));
    } else {
        // libarchive does not always expose packed sizes; do not claim 100%.
        // libarchive 未必提供压缩后大小；不要因此显示 100%。
        m_statusSummaryLeft->setText(
            tr("%1 · Unpacked %2").arg(format, MainWindowHelpers::formatFileSize(unpacked)));
    }
    m_statusSummaryRight->setText(
        tr("%1 files, %2 folders (%3)")
            .arg(files)
            .arg(folders)
            .arg(MainWindowHelpers::formatFileSize(unpacked)));
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
        Icons::modern(QStringLiteral("extract.png")), tr("Extract Selected..."));
    extractSelected->setEnabled(backend && !selected.isEmpty() && !busy);
    connect(extractSelected, &QAction::triggered, this, &MainWindow::onExtractSelected);

    QAction *oneClick = menu.addAction(
        Icons::modern(QStringLiteral("oneclick.png")), tr("One-click Extract"));
    oneClick->setEnabled(backend && !busy);
    connect(oneClick, &QAction::triggered, this, &MainWindow::onOneClickExtract);

    menu.addSeparator();

    QAction *addFiles = menu.addAction(
        Icons::modern(QStringLiteral("add.png")), tr("Add Files..."));
    addFiles->setEnabled(backend && ArchiveManager::instance().canModifyCurrentArchive()
                         && !busy);
    connect(addFiles, &QAction::triggered, this, &MainWindow::onAddFiles);

    QAction *deleteSelected = menu.addAction(
        Icons::modern(QStringLiteral("delete.png")), tr("Delete Selected"));
    deleteSelected->setEnabled(backend
                               && ArchiveManager::instance().canModifyCurrentArchive()
                               && !selected.isEmpty() && !busy);
    connect(deleteSelected, &QAction::triggered, this, &MainWindow::onDeleteSelected);

    menu.addSeparator();

    QAction *testArchive = menu.addAction(
        Icons::modern(QStringLiteral("test.png")), tr("Test Archive"));
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
