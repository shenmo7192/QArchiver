#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPointer>

#include "core/ArchiveEntry.h"
#include "core/ArchiveError.h"
#include "core/ArchiveOptions.h"

class FileListModel;
class FilterProxyModel;
class ArchiveTreeView;
class ProgressDialog;
class ArchiveBackend;
class QStackedWidget;
class QDockWidget;
class QToolBar;

class QAction;
class QLabel;
class QLineEdit;
class QProgressBar;

// Main application window (design doc 3/6/7)
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onOpen();
    void onNewArchive();
    void onExtractAll();
    void onExtractSelected();
    void onAddFiles();
    void onDeleteSelected();
    void onPreferences();
    void onProperties();
    void onCloseArchive();
    void onViewLog();
    void onAbout();
    void onBookmarkCurrent();

    void onArchiveOpening(const QString &filePath);
    void onArchiveOpened(const QString &filePath);
    void onFileListReady(const QList<ArchiveEntry> &entries);
    void onProgressChanged(int percent, int total, int current, int etaSec);
    void onOutputReady(const QString &line);
    void onPasswordRequested();
    void onErrorOccurred(ArchiveErrorCode code, const QString &msg);
    void onOperationFinished(bool success, const QString &msg);

    void onEntryDoubleClicked(const QString &path);
    void onTreeContextMenu(const QPoint &pos);
    void onSearchChanged(const QString &text);
    void onSidebarFileOpened(const QString &path);

private:
    void setupActions();
    void setupToolbar();
    void setupCentralWidget();
    void setupSidebar();
    void applyContentViewMode();
    void applyToolButtonStyle();
    void updateActionsState();
    void showPlaceholder(const QString &message);
    void showProgressDialog(const QString &title);
    void setBusyIndicatorVisible(bool visible, const QString &text = QString());
    QStringList selectedEntryPaths() const;
    QStringList chooseSourceItems(const QString &title);
    void openFile(const QString &path);
    void startExtract(const ExtractOptions &opts);
    void addFilesToCurrentArchive(const QStringList &files);

    // Models
    FileListModel *m_model = nullptr;
    FilterProxyModel *m_proxyModel = nullptr;

    // Views
    ArchiveTreeView *m_treeView = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_placeholderLabel = nullptr;
    QStackedWidget *m_centralStack = nullptr;
    QDockWidget *m_sidebarDock = nullptr;
    QToolBar *m_mainToolBar = nullptr;

    // Actions
    QAction *m_openAct = nullptr;
    QAction *m_newAct = nullptr;
    QAction *m_extractAct = nullptr;
    QAction *m_addAct = nullptr;
    QAction *m_deleteAct = nullptr;
    QAction *m_propertiesAct = nullptr;
    QAction *m_closeAct = nullptr;
    QAction *m_bookmarkAct = nullptr;

    // Dialogs
    QPointer<ProgressDialog> m_progressDialog;

    // Busy indicator in the status bar
    QWidget *m_busyWidget = nullptr;
    QLabel *m_busyLabel = nullptr;
    QProgressBar *m_busyBar = nullptr;

    // State
    QString m_currentPassword;   // Password for the current archive
    QString m_sessionPassword;   // Optional session-wide remembered password
    bool m_openDestAfterExtract = false;
    QString m_lastExtractDest;
    enum class UiOp { None, Open, List, Extract, Compress, Add, Delete };
    UiOp m_currentOp = UiOp::None;
};

#endif // MAINWINDOW_H
