cmake_minimum_required(VERSION 3.20)

project(QArchiver
    VERSION 0.0.1
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt search path: AGENTS.md requires Qt 6.11.2 at /opt/lib/qt6.11
# PREPEND so this path is searched before any system Qt installation.
list(PREPEND CMAKE_PREFIX_PATH "/opt/lib/qt6.11")

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Widgets Concurrent LinguistTools DBus)

qt_standard_project_setup()

# Internationalization
set(TS_FILES
    ${CMAKE_SOURCE_DIR}/translations/zh_CN.ts
    ${CMAKE_SOURCE_DIR}/translations/en.ts
)

set(QM_FILES)
if(Qt6LinguistTools_FOUND)
    qt6_add_translation(QM_FILES ${TS_FILES})
    # Ensure .qm files are built as part of the default build.
    add_custom_target(translations ALL DEPENDS ${QM_FILES})
endif()

# Source files
set(PROJECT_SOURCES
    src/main.cpp

    # core
    src/core/ArchiveManager.cpp
    src/core/ArchiveWorker.cpp
    src/core/ArchiveError.cpp
    src/core/AppSettings.cpp
    src/core/RecentFilesManager.cpp
    src/core/BookmarksManager.cpp

    # backends
    src/backends/ArchiveBackend.cpp
    src/backends/tar/TarBackend.cpp
    src/backends/tar/TarOutputParser.cpp
    src/backends/zip/ZipBackend.cpp
    src/backends/sevenzip/SevenZipBackend.cpp
    src/backends/rar/RarBackend.cpp
    src/backends/stream/StreamBackend.cpp

    # ui
    src/ui/MainWindow.cpp
    src/ui/FileListModel.cpp
    src/ui/FilterProxyModel.cpp
    src/ui/FileListViewDelegate.cpp
    src/ui/PreviewManager.cpp
    src/ui/dialogs/ExtractDialog.cpp
    src/ui/dialogs/CompressDialog.cpp
    src/ui/dialogs/PasswordDialog.cpp
    src/ui/dialogs/ProgressDialog.cpp
    src/ui/dialogs/PreferencesDialog.cpp
    src/ui/dialogs/ArchivePropertiesDialog.cpp
    src/ui/widgets/ArchiveTreeView.cpp

    # utils
    src/utils/ProcessHelper.cpp
    src/utils/FileUtils.cpp
    src/utils/EncodingDetector.cpp
    src/utils/MimeTypeHelper.cpp
    src/utils/ThemeManager.cpp
    src/utils/Logger.cpp
)

# Resource files
set(PROJECT_RESOURCES
    src/resources/resources.qrc
)

qt_add_resources(PROJECT_SOURCES ${PROJECT_RESOURCES})

qt_add_executable(QArchiver
    ${PROJECT_SOURCES}
)

target_include_directories(QArchiver PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/core
    ${CMAKE_SOURCE_DIR}/src/backends
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/utils
)

target_link_libraries(QArchiver PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Concurrent
    Qt6::DBus
)

# Install
install(TARGETS QArchiver
    RUNTIME DESTINATION bin
)

install(FILES ${QM_FILES}
    DESTINATION share/qarchiver/translations
)
