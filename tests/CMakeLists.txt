# Test suite: unit and integration tests for the core, backends and UI wiring.
# 测试套件：覆盖核心层、后端与 UI 连接方式的单元及集成测试。

# Qt Test is optional; the application itself does not require it.
# Qt Test 是可选依赖；应用本身不需要它。
find_package(Qt6 ${QARCHIVER_MIN_QT_VERSION} QUIET COMPONENTS Core Gui Widgets Concurrent Test)
if(NOT TARGET Qt6::Test)
    message(STATUS "Qt6::Test not found: skipping QArchiver tests")
    return()
endif()

# Enable moc/automoc for the standalone test executables.
# 为独立测试可执行文件启用 moc/automoc。
set(CMAKE_AUTOMOC ON)

# Utility tests: path normalization, traversal checks and -slt parsing.
# 工具测试：路径规范化、路径穿越检查与 -slt 解析。
qt_add_executable(tst_utils
    tst_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.cpp
)

target_include_directories(tst_utils PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(tst_utils PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME tst_utils COMMAND tst_utils)

# Toolchain discovery: --help validation, system fallback and RAR codec probe.
# 工具链发现：--help 校验、系统回退与 RAR 编解码器探测。
qt_add_executable(tst_toolchain
    tst_toolchain.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.h
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.h
)

target_include_directories(tst_toolchain PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(tst_toolchain PRIVATE
    Qt6::Core
    Qt6::Test
)

add_test(NAME tst_toolchain COMMAND tst_toolchain)

# Background image helpers: alignment, scaling, target rectangles and painting.
# 背景图片辅助函数：对齐、缩放、目标矩形与绘制。
qt_add_executable(tst_background
    tst_background.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/BackgroundImage.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/BackgroundImage.h
)

target_include_directories(tst_background PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(tst_background PRIVATE
    Qt6::Gui
    Qt6::Test
)

add_test(NAME tst_background COMMAND tst_background)
set_tests_properties(tst_background PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Backend tests: tar/zip/7z/RAR/stream round-trips and special names.
# 后端测试：tar/zip/7z/RAR/流往返及特殊文件名。
qt_add_executable(tst_backends
    tst_backends.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/zip/ZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.h
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.h
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.h
    ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.h
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.h
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.h
)

target_include_directories(tst_backends PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(tst_backends PRIVATE
    Qt6::Core
    Qt6::Test
)

# Fixture directory for split-volume tests and other binary archives.
# 分卷测试及其他二进制压缩包样例的目录。
target_compile_definitions(tst_backends PRIVATE
    QARCHIVER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test_files")

add_test(NAME tst_backends COMMAND tst_backends)

# Continuous image preview: same-directory image selection and batch extraction.
# 连续看图：同目录图片选取与批量解压。
qt_add_executable(tst_preview
    tst_preview.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewUtils.h
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.h
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.h
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.h
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.h
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.h
)

target_include_directories(tst_preview PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(tst_preview PRIVATE
    Qt6::Core
    Qt6::Test
)

target_compile_definitions(tst_preview PRIVATE
    QARCHIVER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test_files")

add_test(NAME tst_preview COMMAND tst_preview)

# ArchiveManager tests: async open/list/extract, fallback and preferences.
# ArchiveManager 测试：异步打开/列表/解压、回退与偏好设置。
qt_add_executable(tst_manager
    tst_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewManager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewManager.h
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/PreviewUtils.h
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/ProgressDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/ProgressDialog.h
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveManager.h
    ${CMAKE_SOURCE_DIR}/src/core/AppSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AppSettings.h
    ${CMAKE_SOURCE_DIR}/src/core/RecentFilesManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RecentFilesManager.h
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.h
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/zip/ZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.h
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.h
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.h
    ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.h
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.h
)

target_include_directories(tst_manager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(tst_manager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
)

target_compile_definitions(tst_manager PRIVATE
    QARCHIVER_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test_files")

add_test(NAME tst_manager COMMAND tst_manager)

# libarchive backend: only built when the system library was found.
# libarchive 后端：仅当找到系统库时构建。
if(QARCHIVER_HAS_LIBARCHIVE)
    # Optional libarchive backend tests.
    # 可选的 libarchive 后端测试。
    qt_add_executable(tst_libarchive
        tst_libarchive.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.h
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.h
        ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
        ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.h
        ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.h
        ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.h
        ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.h
        ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/Logger.h
        ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.cpp
        ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.h
    )

    target_include_directories(tst_libarchive PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/core
        ${CMAKE_SOURCE_DIR}/src/backends
        ${CMAKE_SOURCE_DIR}/src/utils
    )

    target_link_libraries(tst_libarchive PRIVATE
        Qt6::Core
        Qt6::Test
        ${QARCHIVER_LIBARCHIVE_TARGET}
    )

    target_compile_definitions(tst_libarchive PRIVATE QARCHIVER_HAS_LIBARCHIVE)

    add_test(NAME tst_libarchive COMMAND tst_libarchive)

    # tst_manager drives ArchiveManager, which embeds the libarchive dispatch
    # whenever the library is available at build time.
    # tst_manager 驱动 ArchiveManager；构建时若库可用，
    # ArchiveManager 会包含 libarchive 调度逻辑。
    target_sources(tst_manager PRIVATE
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.h
    )
    target_compile_definitions(tst_manager PRIVATE QARCHIVER_HAS_LIBARCHIVE)
    target_link_libraries(tst_manager PRIVATE ${QARCHIVER_LIBARCHIVE_TARGET})

endif()

# Drag-out staging (DragOutManager): needs the manager, AppSettings and the
# backends, but no widgets - it runs headless.
# 拖出暂存（DragOutManager）：需要管理器、AppSettings 和后端，
# 但不依赖控件，可无界面运行。
# Drag-out tests: staging, path mapping, caching and CLI fallback.
# 拖出测试：暂存、路径映射、缓存与 CLI 回退。
qt_add_executable(tst_dragout
    tst_dragout.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DragOutManager.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/DragOutManager.h
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveManager.h
    ${CMAKE_SOURCE_DIR}/src/core/AppSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AppSettings.h
    ${CMAKE_SOURCE_DIR}/src/core/RecentFilesManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RecentFilesManager.h
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.h
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/zip/ZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.h
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.h
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.h
    ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.h
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.h
)

target_include_directories(tst_dragout PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(tst_dragout PRIVATE
    Qt6::Core
    Qt6::Concurrent
    Qt6::Test
)

add_test(NAME tst_dragout COMMAND tst_dragout)

# The drag-out staging must also work with the in-process libarchive backend.
# 拖出暂存也必须支持进程内 libarchive 后端。
if(QARCHIVER_HAS_LIBARCHIVE)
    target_sources(tst_dragout PRIVATE
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.h
    )
    target_compile_definitions(tst_dragout PRIVATE QARCHIVER_HAS_LIBARCHIVE)
    target_link_libraries(tst_dragout PRIVATE ${QARCHIVER_LIBARCHIVE_TARGET})
endif()

# Preferences dialog wiring (needs widgets, runs with the offscreen platform).
# 偏好设置对话框连接测试（需要控件，使用离屏平台运行）。
# Preferences and password dialog tests (offscreen QPA).
# 偏好设置与密码对话框测试（离屏 QPA）。
qt_add_executable(tst_preferences
    tst_preferences.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/PasswordDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/PasswordDialog.h
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/PreferencesDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/PreferencesDialog.h
    ${CMAKE_SOURCE_DIR}/src/ui/FileListModel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/FileListModel.h
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveManager.h
    ${CMAKE_SOURCE_DIR}/src/core/AppSettings.cpp
    ${CMAKE_SOURCE_DIR}/src/core/AppSettings.h
    ${CMAKE_SOURCE_DIR}/src/core/RecentFilesManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/RecentFilesManager.h
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ArchiveError.h
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipListParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipUtils.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/zip/ZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/SevenZipTool.h
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.h
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/EncodingDetector.h
    ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/MimeTypeHelper.h
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/Logger.h
    ${CMAKE_SOURCE_DIR}/src/utils/ThemeManager.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ThemeManager.h
)

target_include_directories(tst_preferences PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(tst_preferences PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::Test
)

add_test(NAME tst_preferences COMMAND tst_preferences)
set_tests_properties(tst_preferences PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# libarchive sources for the preferences test (target defined above).
# 偏好设置测试使用的 libarchive 源码（目标已在上方定义）。
if(QARCHIVER_HAS_LIBARCHIVE)
    target_sources(tst_preferences PRIVATE
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.h
    )
    target_compile_definitions(tst_preferences PRIVATE QARCHIVER_HAS_LIBARCHIVE)
    target_link_libraries(tst_preferences PRIVATE ${QARCHIVER_LIBARCHIVE_TARGET})
endif()

# Make the bundled 7-Zip helper visible to every test that spawns a CLI
# backend.  QARCHIVER_7Z_PATH is the first entry in
# ProcessRunner::resolveToolPath(), so tests use exactly the binary that ships
# with QArchiver even when no system 7z is installed.
# 让每个会启动 CLI 后端的测试都能找到随程序分发的 7-Zip 辅助程序。
# QARCHIVER_7Z_PATH 是 ProcessRunner::resolveToolPath() 的第一优先级，
# 因此即使系统未安装 7z，测试也会使用 QArchiver 自带的二进制。
if(QARCHIVER_BUNDLE_7ZIP)
    set(_qarchiver_test_7z_env "QARCHIVER_7Z_PATH=${QARCHIVER_7ZIP_EXE}")
    foreach(_qarchiver_test IN ITEMS
            tst_utils tst_toolchain tst_backends tst_preview tst_manager
            tst_libarchive tst_dragout tst_preferences)
        if(TEST ${_qarchiver_test})
            set_tests_properties(${_qarchiver_test} PROPERTIES
                ENVIRONMENT "${_qarchiver_test_7z_env}")
        endif()
    endforeach()
    if(TEST tst_preferences)
        set_tests_properties(tst_preferences PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${_qarchiver_test_7z_env}")
    endif()
endif()
