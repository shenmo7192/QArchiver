# Core layer: settings, error types, archive manager and recent files.
# 核心层：设置、错误类型、压缩包管理器与最近文件。
target_sources(QArchiver PRIVATE
    # Controller and settings.
    # 控制器与设置。
    ArchiveManager.cpp
    ArchiveManager.h
    ArchiveError.cpp
    ArchiveError.h
    AppSettings.cpp
    AppSettings.h
    RecentFilesManager.cpp
    RecentFilesManager.h

    # Shared value types used by the UI and all backends.
    # UI 与所有后端共用的值类型。
    ArchiveConstants.h
    ArchiveEntry.h
    ArchiveOptions.h
)
