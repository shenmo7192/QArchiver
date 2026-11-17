cmake_minimum_required(VERSION 3.20)

project(QArchiver
    VERSION 0.1.0
    LANGUAGES CXX
)

# C++20 baseline for the non-GUI core/backends/utils layers.
# Qt 6.8 itself requires at least C++17 and supports C++20 with g++ 12+.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt 6.8 from Debian 13's qt6-base-dev package. CMake finds it under /usr
# automatically; for a custom Qt prefix pass -DCMAKE_PREFIX_PATH=<qt-prefix>.
find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent)

# Modules are probed one at a time: a missing optional module must not prevent
# the others from being detected (Qt6's config package skips all remaining
# components when one component in a list is absent).
foreach(component IN ITEMS LinguistTools DBus Test)
    find_package(Qt6 6.8 QUIET COMPONENTS ${component})
endforeach()

# Translations are built exclusively through qt_add_translations(), which needs
# the Linguist tools (lupdate/lrelease) shipped by qt6-tools-dev.
if(NOT Qt6LinguistTools_FOUND)
    message(FATAL_ERROR
        "Qt6LinguistTools not found: install qt6-tools-dev (or the equivalent "
        "package providing lupdate/lrelease) so qt_add_translations() can build "
        "and embed the .qm translations.")
endif()

qt_standard_project_setup(REQUIRES 6.8)
# Compile src/resources/resources.qrc (the split target_sources(qrc) setup
# does not enable AUTORCC by itself).
set(CMAKE_AUTORCC ON)

include(GNUInstallDirs)
include(cmake/QArchiverWarnings.cmake)

# libarchive powers the read-only LibArchiveBackend (zip/tar/7z/streams without
# spawning 7z). It is optional: when the headers are missing the build falls back
# to the command-line backends only.
option(QARCHIVER_USE_LIBARCHIVE
    "Use the system libarchive for read-only archive access" ON)

if(QARCHIVER_USE_LIBARCHIVE)
    # Prefer the CMake config/FindLibArchive module, then pkg-config (some
    # distributions ship the .pc file without FindLibArchive.cmake).
    find_package(LibArchive 3.4)
    if(NOT LibArchive_FOUND)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(LibArchivePC QUIET IMPORTED_TARGET libarchive>=3.4)
            if(LibArchivePC_FOUND)
                set(QARCHIVER_LIBARCHIVE_TARGET PkgConfig::LibArchivePC)
                set(LibArchive_VERSION ${LibArchivePC_VERSION})
                set(LibArchive_FOUND TRUE)
            endif()
        endif()
    else()
        set(QARCHIVER_LIBARCHIVE_TARGET LibArchive::LibArchive)
    endif()
endif()

if(QARCHIVER_USE_LIBARCHIVE AND LibArchive_FOUND)
    set(QARCHIVER_HAS_LIBARCHIVE ON)
    message(STATUS "libarchive found (${LibArchive_VERSION}): read-only backend enabled")
    message(STATUS "  target: ${QARCHIVER_LIBARCHIVE_TARGET}")
elseif(QARCHIVER_USE_LIBARCHIVE)
    message(WARNING
        "libarchive not found: building without the libarchive backend. "
        "Install libarchive-dev (Debian/Ubuntu) or use -DQARCHIVER_USE_LIBARCHIVE=OFF.")
endif()

add_subdirectory(src)

add_subdirectory(translations)

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
