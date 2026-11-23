# Utility layer: process execution, paths, encoding, themes and logging.
# 工具层：进程执行、路径、编码、主题与日志。
target_sources(QArchiver PRIVATE
    # External process execution and archive tool discovery.
    # 外部进程执行与压缩工具查找。
    ProcessRunner.cpp
    ProcessRunner.h
    PlatformProcess.h
    SevenZipTool.cpp
    SevenZipTool.h

    # Filesystem and archive path helpers.
    # 文件系统与压缩包路径辅助函数。
    FileUtils.cpp
    FileUtils.h

    # Filename encoding detection and conversion.
    # 文件名编码检测与转换。
    EncodingDetector.cpp
    EncodingDetector.h

    # MIME/format detection and file-kind presentation.
    # MIME/格式检测与文件类型展示。
    MimeTypeHelper.cpp
    MimeTypeHelper.h
    FileTypeHelper.cpp
    FileTypeHelper.h

    # Application theme and logging support.
    # 应用主题与日志支持。
    ThemeManager.cpp
    ThemeManager.h
    BackgroundImage.cpp
    BackgroundImage.h
    Logger.cpp
    Logger.h
    IconUtils.h
)
