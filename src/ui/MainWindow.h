#ifndef MAINWINDOW_H
#define MAINWINDOW_H

// QArchiver / 小Q解压
// Main application window and its user interaction state.
// 主应用窗口及其用户交互状态。

#include <QMainWindow>
#include <QPointer>

#include "core/ArchiveEntry.h"
#include "core/ArchiveError.h"
#include "core/ArchiveOptions.h"

class FileListModel;
class FilterProxyModel;
class ArchiveTreeView;
class BackgroundWidget;
class ProgressDialog;
class ArchiveBackend;
class QStackedWidget;
class QDockWidget;
class QToolBar;

struct SevenZipToolInfo;

class QAction;
class QLabel;
class QLineEdit;
class QProgressBar;

// Main application window (design doc 3/6/7).
// 主应用窗口（设计文档 3/6/7）。
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onOpen();
    void onNewArchive();
    void onExtractAll();
    void onExtractSelected();
    void onAddFiles();
    void onDeleteSelected();
    void onPreferences();
    void onCloseArchive();
    void onAbout();
    void checkSevenZipFallback();
    void showSevenZipFallbackInfo(const SevenZipToolInfo &info);

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
    void onEntriesDragOut(const QStringList &paths);

private:
    void setupActions();
    void setupToolbar();
    void setupCentralWidget();
    void setupSidebar();
    void applyContentViewMode();
    void applyToolButtonStyle();
    void applySidebarVisibility();
    void updateSearchIcon();
    void updatePlaceholderBackground();
    void updateActionsState();
    void showPlaceholder(const QString &message);
    void showProgressDialog(const QString &title);
    void setBusyIndicatorVisible(bool visible, const QString &text = QString());
    QStringList selectedEntryPaths() const;
    QStringList chooseSourceItems(const QString &title);
    void openFile(const QString &path);
    void startExtract(const ExtractOptions &opts);
    void addFilesToCurrentArchive(const QStringList &files);

    // Models / 模型
    FileListModel *m_model = nullptr;
    FilterProxyModel *m_proxyModel = nullptr;

    // Views / 视图
    ArchiveTreeView *m_treeView = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_placeholderLabel = nullptr;
    QAction *m_searchAction = nullptr;
    QStackedWidget *m_centralStack = nullptr;
    BackgroundWidget *m_backgroundWidget = nullptr;
    QDockWidget *m_sidebarDock = nullptr;
    QToolBar *m_mainToolBar = nullptr;

    // Actions / 动作
    QAction *m_openAct = nullptr;
    QAction *m_newAct = nullptr;
    QAction *m_extractAct = nullptr;
    QAction *m_addAct = nullptr;
    QAction *m_deleteAct = nullptr;
    QAction *m_closeAct = nullptr;

    // Dialogs / 对话框
    QPointer<ProgressDialog> m_progressDialog;

    // Busy indicator in the status bar / 状态栏忙碌指示器
    QWidget *m_busyWidget = nullptr;
    QLabel *m_busyLabel = nullptr;
    QProgressBar *m_busyBar = nullptr;

    // State / 状态
    QString m_currentPassword;   // Password for the current archive / 当前压缩包密码
    bool m_dragPrepareInProgress = false;  // Guard against re-entrant drags / 防止重入拖拽
    bool m_selfDropAttempted = false;      // Entry dropped back onto ourselves / 条目被拖回自身
    QString m_sessionPassword;   // Optional session-wide remembered password / 可选的会话级记住密码
    bool m_openDestAfterExtract = false;
    QString m_lastExtractDest;
    enum class UiOp { None, Open, List, Extract, Compress, Add, Delete };
    UiOp m_currentOp = UiOp::None;
};

#endif // MAINWINDOW_H
