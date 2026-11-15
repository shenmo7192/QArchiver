cmake_minimum_required(VERSION 3.21)

project(QArchiver
    VERSION 0.1.0
    LANGUAGES CXX
)

# Qt search path: AGENTS.md requires Qt 6.11.2 at /opt/lib/qt6.11.
list(PREPEND CMAKE_PREFIX_PATH "/opt/lib/qt6.11")

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
find_package(Qt6 6.11 QUIET COMPONENTS LinguistTools DBus Test)

qt_standard_project_setup(REQUIRES 6.11)
# Compile src/resources/resources.qrc (the split target_sources(qrc) setup
# does not enable AUTORCC by itself).
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
include(cmake/QArchiverWarnings.cmake)

add_subdirectory(src)

if(Qt6LinguistTools_FOUND)
    add_subdirectory(translations)
endif()

include(CTest)
if(BUILD_TESTING AND TARGET Qt6::Test)
    add_subdirectory(tests)
endif()

install(FILES packaging/qarchiver.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES src/resources/icons/QArchiver.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
    RENAME qarchiver.png
)
