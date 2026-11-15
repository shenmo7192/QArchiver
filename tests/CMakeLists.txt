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
