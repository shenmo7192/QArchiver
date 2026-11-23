include_guard(GLOBAL)

# Optional in-process libarchive discovery.
# 可选的进程内 libarchive 探测。
#
# libarchive powers the read-only LibArchiveBackend (zip/tar/7z/streams without
# spawning 7z).  It is optional: without it QArchiver falls back to the
# command-line backends only.
# libarchive 用于只读的 LibArchiveBackend（无需启动 7z 即可读取 zip/tar/7z/流），
# 它是可选的；缺少时 QArchiver 仅使用命令行后端。

option(QARCHIVER_USE_LIBARCHIVE
    "Use the system libarchive for read-only archive access" ON)

# Detect libarchive once, at the repository root, and publish:
#   QARCHIVER_HAS_LIBARCHIVE     - ON when a usable target exists
#   QARCHIVER_LIBARCHIVE_TARGET  - imported target to link against
# The result is a plain variable, so every subdirectory (src/, tests/) can read
# it without passing values upwards again.
# 在仓库根目录探测一次 libarchive，并输出上述两个变量。结果是普通变量，
# 因此所有子目录（src/、tests/）都能直接读取，无需再逐层向上传递。
function(qarchiver_find_libarchive)
    set(_target "")

    if(QARCHIVER_USE_LIBARCHIVE)
        # Prefer the CMake config/FindLibArchive module, then pkg-config
        # because some distributions ship only the .pc file.
        # 优先使用 CMake 的配置/FindLibArchive 模块，其次使用 pkg-config，
        # 因为部分发行版只提供 .pc 文件。
        find_package(LibArchive 3.4)
        if(LibArchive_FOUND)
            set(_target LibArchive::LibArchive)
        else()
            find_package(PkgConfig QUIET)
            if(PkgConfig_FOUND)
                pkg_check_modules(LibArchivePC QUIET
                    IMPORTED_TARGET libarchive>=3.4)
                if(LibArchivePC_FOUND)
                    set(_target PkgConfig::LibArchivePC)
                    set(LibArchive_VERSION "${LibArchivePC_VERSION}")
                endif()
            endif()
        endif()
    endif()

    if(_target)
        # Imported targets are directory-scoped by default.  Promote libarchive
        # so src/ and tests/ can link to the target found here.
        # 导入目标默认只在当前目录可见；将其提升为全局目标，
        # 使 src/ 与 tests/ 能链接这里找到的目标。
        get_target_property(_libarchive_imported ${_target} IMPORTED)
        if(_libarchive_imported)
            set_target_properties(${_target} PROPERTIES IMPORTED_GLOBAL TRUE)
        endif()
        set(QARCHIVER_HAS_LIBARCHIVE ON PARENT_SCOPE)
        set(QARCHIVER_LIBARCHIVE_TARGET "${_target}" PARENT_SCOPE)
        message(STATUS
            "libarchive found (${LibArchive_VERSION}): read-only backend enabled")
        message(STATUS "  target: ${_target}")
        return()
    endif()

    set(QARCHIVER_HAS_LIBARCHIVE OFF PARENT_SCOPE)
    set(QARCHIVER_LIBARCHIVE_TARGET "" PARENT_SCOPE)
    if(QARCHIVER_USE_LIBARCHIVE)
        message(WARNING
            "libarchive not found: building without the libarchive backend. "
            "Install libarchive-dev (Debian/Ubuntu) or use "
            "-DQARCHIVER_USE_LIBARCHIVE=OFF.")
    endif()
endfunction()
