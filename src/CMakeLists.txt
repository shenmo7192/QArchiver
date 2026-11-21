# Qt runtime dependencies for the application target.
# 应用目标所需的 Qt 运行时依赖。
find_package(Qt6 ${QARCHIVER_MIN_QT_VERSION} REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# Optional Qt modules are probed individually: a missing optional module must
# not prevent the other modules from being detected.
# 可选 Qt 模块逐个探测：缺少某个可选模块不应影响其他模块的检测。
foreach(_qt_component IN ITEMS DBus)
    find_package(Qt6 ${QARCHIVER_MIN_QT_VERSION} QUIET COMPONENTS ${_qt_component})
endforeach()

# GNUInstallDirs provides the standard bin/lib/share install destinations.
# GNUInstallDirs 提供标准的 bin/lib/share 安装目录变量。
include(GNUInstallDirs)

# Standard Qt project defaults (AUTOMOC, AUTOUIC, output directories, ...).
# Qt 项目标准默认设置（AUTOMOC、AUTOUIC、输出目录等）。
qt_standard_project_setup(REQUIRES ${QARCHIVER_MIN_QT_VERSION})

# Main application target; sources are contributed by the child directories.
# 主应用目标；具体源文件由各子目录追加。
qt_add_executable(QArchiver
    main.cpp
)

target_compile_definitions(QArchiver PRIVATE
    QARCHIVER_VERSION="${PROJECT_VERSION}"
)

target_include_directories(QArchiver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(QArchiver PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)

# Source groups.  backends/ also detects libarchive and publishes the result.
# 源码分组。backends/ 还会探测 libarchive，并把结果传递给上层。
add_subdirectory(core)
add_subdirectory(backends)
add_subdirectory(ui)
add_subdirectory(utils)
add_subdirectory(resources)

# The optional in-process libarchive backend is linked only when available.
# 仅在系统提供 libarchive 时链接可选的进程内 libarchive 后端。
if(QARCHIVER_HAS_LIBARCHIVE)
    target_link_libraries(QArchiver PRIVATE ${QARCHIVER_LIBARCHIVE_TARGET})
    target_compile_definitions(QArchiver PRIVATE QARCHIVER_HAS_LIBARCHIVE)
endif()

# D-Bus is used for desktop integration when the Qt module is installed.
# 当系统安装 Qt D-Bus 模块时，用于桌面环境集成。
if(TARGET Qt6::DBus)
    target_link_libraries(QArchiver PRIVATE Qt6::DBus)
    target_compile_definitions(QArchiver PRIVATE QARCHIVER_HAS_DBUS)
endif()

qarchiver_set_warnings(QArchiver)
qarchiver_bundle_7zip(QArchiver)

install(TARGETS QArchiver
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install the bundled 7-Zip binaries and their license next to the app target.
# 安装随程序分发的 7-Zip 二进制及其许可证。
qarchiver_install_7zip()

# Publish dependency results to the root directory so tests can reuse them.
# 将依赖探测结果传递到根目录，供测试复用。
set(QARCHIVER_HAS_LIBARCHIVE "${QARCHIVER_HAS_LIBARCHIVE}" PARENT_SCOPE)
set(QARCHIVER_LIBARCHIVE_TARGET "${QARCHIVER_LIBARCHIVE_TARGET}" PARENT_SCOPE)
