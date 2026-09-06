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

class QAction;
class QLabel;
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
    void onPreferences();
    void onProperties();
    void onCloseArchive();
    void onViewLog();
    void onAbout();

    void onArchiveOpened(const QString &filePath);
    void onFileListReady(const QList<ArchiveEntry> &entries);
    void onProgressChanged(int percent, int total, int current, int etaSec);
    void onOutputReady(const QString &line);
    void onPasswordRequested();
    void onErrorOccurred(ArchiveErrorCode code, const QString &msg);
    void onOperationFinished(bool success, const QString &msg);

    void onEntryDoubleClicked(const QString &path);

private:
    void setupActions();
    void setupToolbar();
    void setupCentralWidget();
    void updateActionsState();
    void showPlaceholder(const QString &message);
    void showProgressDialog(const QString &title);
    void setBusyIndicatorVisible(bool visible, const QString &text = QString());
    QStringList selectedEntryPaths() const;
    void openFile(const QString &path);
    void startExtract(const ExtractOptions &opts);

    // Models
    FileListModel *m_model = nullptr;
    FilterProxyModel *m_proxyModel = nullptr;

    // Views
    ArchiveTreeView *m_treeView = nullptr;
    QLabel *m_placeholderLabel = nullptr;
    QStackedWidget *m_centralStack = nullptr;

    // Actions
    QAction *m_openAct = nullptr;
    QAction *m_newAct = nullptr;
    QAction *m_extractAct = nullptr;
    QAction *m_propertiesAct = nullptr;
    QAction *m_closeAct = nullptr;

    // Dialogs
    QPointer<ProgressDialog> m_progressDialog;

    // Busy indicator in the status bar (shown while listing an archive)
    QWidget *m_busyWidget = nullptr;
    QLabel *m_busyLabel = nullptr;
    QProgressBar *m_busyBar = nullptr;

    // State
    QString m_currentPassword;  // Session-level password cache
    enum class UiOp { None, Open, List, Extract, Compress, Add };
    UiOp m_currentOp = UiOp::None;
};

#endif // MAINWINDOW_H
