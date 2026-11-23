include_guard(GLOBAL)

# Windows-specific executable metadata and deployment.
# Windows 专用可执行文件元数据与部署规则。

option(QARCHIVER_WINDOWS_DEPLOY
    "Install the Qt runtime for Windows builds (disable for cross-compile checks)"
    ON)

# Embed the application icon, version resource and DPI/long-path manifest.
# 嵌入应用图标、版本资源以及 DPI/长路径清单。
function(qarchiver_add_windows_resources target)
    if(NOT WIN32)
        return()
    endif()

    set(_icon "${PROJECT_SOURCE_DIR}/resources/app-icons/qarchiver.ico")
    set(_manifest "${PROJECT_SOURCE_DIR}/packaging/windows/QArchiver.manifest")
    if(NOT EXISTS "${_icon}")
        message(WARNING
            "Windows icon not found: ${_icon}. "
            "Run tools/gen_modern_icons.py to generate it.")
    endif()

    set(QARCHIVER_WINDOWS_ICON "${_icon}")
    set(QARCHIVER_WINDOWS_MANIFEST "${_manifest}")
    configure_file(
        "${PROJECT_SOURCE_DIR}/packaging/windows/QArchiver.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/QArchiver.rc"
        @ONLY
    )
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/QArchiver.rc")

    # A GUI executable: no console window is attached on Windows.
    # GUI 可执行文件：Windows 上不附带控制台窗口。
    set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE TRUE)
endfunction()

# Install the Qt runtime, plugins and compiler runtime next to QArchiver.exe.
# 将 Qt 运行库、插件与编译器运行库安装到 QArchiver.exe 旁。
function(qarchiver_configure_windows_deploy target)
    if(NOT WIN32 OR NOT QARCHIVER_WINDOWS_DEPLOY)
        return()
    endif()

    qt_generate_deploy_app_script(
        TARGET ${target}
        OUTPUT_SCRIPT QARCHIVER_WINDOWS_DEPLOY_SCRIPT
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${QARCHIVER_WINDOWS_DEPLOY_SCRIPT})

    if(MINGW)
        # Qt's deploy tool does not always copy the MinGW runtime DLLs.
        # Qt 的部署工具不一定会复制 MinGW 运行库。
        get_filename_component(_mingw_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
        install(FILES
            "${_mingw_bin}/libstdc++-6.dll"
            "${_mingw_bin}/libgcc_s_seh-1.dll"
            "${_mingw_bin}/libwinpthread-1.dll"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
            OPTIONAL
        )
    endif()
endfunction()
