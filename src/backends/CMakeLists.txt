# Backend sources shared by the application and the CLI fallback.
# 应用和命令行回退后端共用的源码。
target_sources(QArchiver PRIVATE
    ArchiveBackend.cpp
    ArchiveBackend.h

    tar/TarBackend.cpp
    tar/TarBackend.h

    zip/ZipBackend.h

    sevenzip/SevenZipBackend.cpp
    sevenzip/SevenZipBackend.h
    sevenzip/SevenZipListParser.cpp
    sevenzip/SevenZipListParser.h
    sevenzip/SevenZipUtils.cpp
    sevenzip/SevenZipUtils.h

    rar/RarBackend.cpp
    rar/RarBackend.h

    stream/StreamBackend.cpp
    stream/StreamBackend.h
)

# libarchive powers the read-only LibArchiveBackend (zip/tar/7z/streams without
# spawning 7z).  It is optional: without it QArchiver falls back to the
# command-line backends only.
# libarchive 用于只读的 LibArchiveBackend（无需启动 7z 即可读取 zip/tar/7z/流），
# 它是可选的；缺少时 QArchiver 仅使用命令行后端。
option(QARCHIVER_USE_LIBARCHIVE
    "Use the system libarchive for read-only archive access" ON)

if(QARCHIVER_USE_LIBARCHIVE)
    # Prefer the CMake config/FindLibArchive module, then pkg-config because
    # some distributions ship only the .pc file.
    # 优先使用 CMake 的配置/FindLibArchive 模块，其次使用 pkg-config，
    # 因为部分发行版只提供 .pc 文件。
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
    # Imported targets are directory-scoped by default.  Promote libarchive so
    # src/ and tests/ can link to the target found here.
    # 导入目标默认只在当前目录可见；将 libarchive 提升为全局目标，
    # 使 src/ 和 tests/ 能链接这里找到的目标。
    get_target_property(_libarchive_imported
        ${QARCHIVER_LIBARCHIVE_TARGET} IMPORTED)
    if(_libarchive_imported)
        set_target_properties(${QARCHIVER_LIBARCHIVE_TARGET} PROPERTIES
            IMPORTED_GLOBAL TRUE)
    endif()
    message(STATUS "libarchive found (${LibArchive_VERSION}): read-only backend enabled")
    message(STATUS "  target: ${QARCHIVER_LIBARCHIVE_TARGET}")
else()
    set(QARCHIVER_HAS_LIBARCHIVE OFF)
    if(QARCHIVER_USE_LIBARCHIVE)
        message(WARNING
            "libarchive not found: building without the libarchive backend. "
            "Install libarchive-dev (Debian/Ubuntu) or use "
            "-DQARCHIVER_USE_LIBARCHIVE=OFF.")
    endif()
endif()

# Add the in-process backend only when a usable libarchive target exists.
# 仅当存在可用的 libarchive 目标时，才加入进程内后端。
if(QARCHIVER_HAS_LIBARCHIVE)
    target_sources(QArchiver PRIVATE
        archive/LibArchiveBackend.cpp
        archive/LibArchiveBackend.h
    )
endif()

# Expose the detection result to src/ and, from there, to tests/.
# 将探测结果传递给 src/，再由 src/ 传递给 tests/。
set(QARCHIVER_HAS_LIBARCHIVE "${QARCHIVER_HAS_LIBARCHIVE}" PARENT_SCOPE)
set(QARCHIVER_LIBARCHIVE_TARGET "${QARCHIVER_LIBARCHIVE_TARGET}" PARENT_SCOPE)
