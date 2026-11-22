# Widgets and dialogs for the main window.
# 主窗口使用的控件与对话框。
target_sources(QArchiver PRIVATE
    # Main window, models, filtering and drag-out support.
    # 主窗口、模型、过滤与拖出支持。
    MainWindow.cpp
    MainWindow.h
    FileListModel.cpp
    FileListModel.h
    FilterProxyModel.cpp
    FilterProxyModel.h
    FileListViewDelegate.cpp
    FileListViewDelegate.h
    PreviewManager.cpp
    PreviewManager.h
    PreviewUtils.cpp
    PreviewUtils.h
    DragOutManager.cpp
    DragOutManager.h

    # Modal dialogs.
    # 模态对话框。
    dialogs/ExtractDialog.cpp
    dialogs/ExtractDialog.h
    dialogs/CompressDialog.cpp
    dialogs/CompressDialog.h
    dialogs/PasswordDialog.cpp
    dialogs/PasswordDialog.h
    dialogs/ProgressDialog.cpp
    dialogs/ProgressDialog.h
    dialogs/PreferencesDialog.cpp
    dialogs/PreferencesDialog.h
    dialogs/BenchmarkDialog.cpp
    dialogs/BenchmarkDialog.h

    # Reusable custom widgets.
    # 可复用自定义控件。
    widgets/ArchiveTreeView.cpp
    widgets/ArchiveTreeView.h
    widgets/BackgroundWidget.cpp
    widgets/BackgroundWidget.h
    widgets/SidebarWidget.cpp
    widgets/SidebarWidget.h
    widgets/ModernToolBar.cpp
    widgets/ModernToolBar.h
    widgets/ModernNavBar.cpp
    widgets/ModernNavBar.h
    widgets/ArchiveFolderPane.cpp
    widgets/ArchiveFolderPane.h
    widgets/ArchiveGridView.cpp
    widgets/ArchiveGridView.h
    widgets/WelcomePage.cpp
    widgets/WelcomePage.h
)
