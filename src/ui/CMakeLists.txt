target_sources(QArchiver PRIVATE
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
    DragOutManager.cpp
    DragOutManager.h

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

    widgets/ArchiveTreeView.cpp
    widgets/ArchiveTreeView.h
    widgets/SidebarWidget.cpp
    widgets/SidebarWidget.h
)
