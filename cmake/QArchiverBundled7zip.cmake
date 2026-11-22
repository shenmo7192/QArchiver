include_guard(GLOBAL)

# Bundle the 7-Zip command-line helper next to QArchiver.  This is the fallback
# backend for formats/operations libarchive cannot handle and removes the hard
# dependency on a system-wide 7z/tar/unrar installation.
# 将 7-Zip 命令行辅助程序随 QArchiver 一起分发。它是 libarchive 无法处理的
# 格式/操作的回退后端，并消除对系统级 7z/tar/unrar 的硬依赖。
option(QARCHIVER_BUNDLE_7ZIP
    "Bundle the 7-Zip command-line helper with QArchiver" ON)

set(QARCHIVER_7ZIP_ROOT
    "${CMAKE_SOURCE_DIR}/third_party/7zip"
    CACHE PATH "Directory containing the bundled 7-Zip binaries")

# Select the binaries for the target platform and expose:
#   QARCHIVER_7ZIP_FILES  - files to copy/install
#   QARCHIVER_7ZIP_EXE    - executable path (used by tests/diagnostics)
# 为目标平台选择二进制文件并输出：
#   QARCHIVER_7ZIP_FILES  - 需要复制/安装的文件
#   QARCHIVER_7ZIP_EXE    - 可执行文件路径（供测试/诊断使用）
function(qarchiver_select_bundled_7zip)
    if(NOT QARCHIVER_BUNDLE_7ZIP)
        return()
    endif()

    if(WIN32)
        if(QARCHIVER_WINDOWS_ARCH STREQUAL "arm64")
            set(_7z_arch "arm64")
        elseif(QARCHIVER_WINDOWS_ARCH STREQUAL "x64")
            set(_7z_arch "x64")
        else()
            message(FATAL_ERROR
                "The bundled 7-Zip helper is only available for Windows x64 "
                "and ARM64. Add third_party/7zip/win/${QARCHIVER_WINDOWS_ARCH} "
                "and extend cmake/QArchiverBundled7zip.cmake for other targets.")
        endif()
        set(_7z_exe "${QARCHIVER_7ZIP_ROOT}/win/${_7z_arch}/7z.exe")
        set(_7z_files
            "${_7z_exe}"
            "${QARCHIVER_7ZIP_ROOT}/win/${_7z_arch}/7z.dll"
        )
    elseif(APPLE)
        message(FATAL_ERROR
            "No bundled 7-Zip binaries for macOS yet. Set "
            "-DQARCHIVER_BUNDLE_7ZIP=OFF or add third_party/7zip/macos/...")
    elseif(UNIX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
            set(_7z_arch arm64)
        elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
            set(_7z_arch arm)
        elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_7z_arch x64)
        else()
            set(_7z_arch x86)
        endif()
        set(_7z_exe "${QARCHIVER_7ZIP_ROOT}/linux/${_7z_arch}/7zzs")
        set(_7z_files "${_7z_exe}")
    else()
        message(FATAL_ERROR "Unsupported platform for the bundled 7-Zip helper")
    endif()

    foreach(_7z_file IN LISTS _7z_files)
        if(NOT EXISTS "${_7z_file}")
            message(FATAL_ERROR "Bundled 7-Zip file not found: ${_7z_file}")
        endif()
    endforeach()

    set(QARCHIVER_7ZIP_FILES "${_7z_files}" PARENT_SCOPE)
    set(QARCHIVER_7ZIP_EXE "${_7z_exe}" PARENT_SCOPE)

    message(STATUS "Bundled 7-Zip helper: ${_7z_exe}")
endfunction()

# Copy the helper into "<target dir>/7zip/" after linking.
# 链接完成后将辅助程序复制到 "<目标目录>/7zip/"。
function(qarchiver_bundle_7zip target)
    if(NOT QARCHIVER_BUNDLE_7ZIP)
        return()
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target}>/7zip"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${QARCHIVER_7ZIP_FILES}
                "$<TARGET_FILE_DIR:${target}>/7zip/"
        COMMENT "Copying bundled 7-Zip helper"
        VERBATIM
    )
endfunction()

# Install rules: Windows keeps the helper next to QArchiver.exe, Linux installs
# it under libexec so /usr/bin is not polluted by a second 7z binary.
# 安装规则：Windows 将辅助程序放在 QArchiver.exe 旁；Linux 安装到 libexec，
# 避免第二个 7z 二进制污染 /usr/bin。
function(qarchiver_install_7zip)
    if(NOT QARCHIVER_BUNDLE_7ZIP)
        return()
    endif()

    if(WIN32)
        install(FILES ${QARCHIVER_7ZIP_FILES}
            DESTINATION ${CMAKE_INSTALL_BINDIR}/7zip)
    else()
        install(PROGRAMS ${QARCHIVER_7ZIP_FILES}
            DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/qarchiver/7zip)
    endif()

    install(FILES "${QARCHIVER_7ZIP_ROOT}/License.txt"
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/qarchiver
        RENAME 7zip-License.txt)
endfunction()
