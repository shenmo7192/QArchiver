cmake_minimum_required(VERSION 3.20)

# QArchiver (小Q解压) is a Qt 6 desktop archive manager.
# QArchiver（小Q解压）是一个基于 Qt 6 的桌面压缩包管理工具。
project(QArchiver
    VERSION 0.1.0
    LANGUAGES CXX
)

# The shared core/backends/utils layers use C++20; Qt 6.4 supports it.
# core/backends/utils 等公共层使用 C++20；Qt 6.4 支持该标准。
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Minimum supported Qt version.  All find_package() calls use this value so
# Qt 6.4+ distributions (Debian 12 / Ubuntu 22.04+) can build QArchiver.
# 最低支持的 Qt 版本。所有 find_package() 都使用该值，使 Qt 6.4+ 的发行版
# （Debian 12 / Ubuntu 22.04+）也能构建 QArchiver。
set(QARCHIVER_MIN_QT_VERSION 6.4)

# Project-wide helper modules.  Feature-specific setup lives in the child
# CMakeLists.txt files; the root only orchestrates the build.
# 项目级辅助模块。具体功能配置放在各子目录的 CMakeLists.txt 中，
# 根脚本只负责组织构建流程。
include(cmake/QArchiverWarnings.cmake)
include(cmake/QArchiverBundled7zip.cmake)

# Select the 7-Zip helper for the target platform before source directories run.
# 在进入源码目录前，为目标平台选择要随程序分发的 7-Zip 辅助程序。
qarchiver_select_bundled_7zip()

# Qt setup, application target and libarchive detection are handled by src/.
# Qt 配置、应用目标与 libarchive 探测由 src/ 负责。
add_subdirectory(src)

# Translations need the Qt Linguist tools and are embedded into QArchiver.
# 翻译需要 Qt Linguist 工具，并会被嵌入 QArchiver。
add_subdirectory(translations)

# Desktop entry and application icon installation.
# 安装桌面入口和应用图标。
add_subdirectory(packaging)

# Tests are optional and find their own Qt Test dependency.
# 测试是可选的，并由测试目录自行查找 Qt Test 依赖。
include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
