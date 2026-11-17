qt_add_executable(tst_utils
    tst_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/FileUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.cpp
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

qt_add_executable(tst_backends
    tst_backends.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/zip/ZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
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

add_test(NAME tst_backends COMMAND tst_backends)

qt_add_executable(tst_manager
    tst_manager.cpp
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
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/zip/ZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
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
    Qt6::Concurrent
    Qt6::Test
)

add_test(NAME tst_manager COMMAND tst_manager)

# libarchive backend: only built when the system library was found.
if(QARCHIVER_HAS_LIBARCHIVE)
    qt_add_executable(tst_libarchive
        tst_libarchive.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/ArchiveBackend.h
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.h
        ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
        ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
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
    target_sources(tst_manager PRIVATE
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.h
    )
    target_compile_definitions(tst_manager PRIVATE QARCHIVER_HAS_LIBARCHIVE)
    target_link_libraries(tst_manager PRIVATE ${QARCHIVER_LIBARCHIVE_TARGET})

endif()

# Drag-out staging (DragOutManager): needs the manager, AppSettings and the
# backends, but no widgets - it runs headless.
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
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/zip/ZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
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
if(QARCHIVER_HAS_LIBARCHIVE)
    target_sources(tst_dragout PRIVATE
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.h
    )
    target_compile_definitions(tst_dragout PRIVATE QARCHIVER_HAS_LIBARCHIVE)
    target_link_libraries(tst_dragout PRIVATE ${QARCHIVER_LIBARCHIVE_TARGET})
endif()

# Preferences dialog wiring (needs widgets, runs with the offscreen platform).
qt_add_executable(tst_preferences
    tst_preferences.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/PreferencesDialog.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/dialogs/PreferencesDialog.h
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
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/tar/TarOutputParser.h
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/sevenzip/SevenZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/rar/RarBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.cpp
    ${CMAKE_SOURCE_DIR}/src/backends/stream/StreamBackend.h
    ${CMAKE_SOURCE_DIR}/src/backends/zip/ZipBackend.h
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/ProcessRunner.h
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
if(QARCHIVER_HAS_LIBARCHIVE)
    target_sources(tst_preferences PRIVATE
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.cpp
        ${CMAKE_SOURCE_DIR}/src/backends/archive/LibArchiveBackend.h
    )
    target_compile_definitions(tst_preferences PRIVATE QARCHIVER_HAS_LIBARCHIVE)
    target_link_libraries(tst_preferences PRIVATE ${QARCHIVER_LIBARCHIVE_TARGET})
endif()
