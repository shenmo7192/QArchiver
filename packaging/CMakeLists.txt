# Desktop integration files installed with QArchiver.
# 随 QArchiver 安装的桌面集成文件。

# Standard install destinations (bin/lib/share).
# 标准安装目录（bin/lib/share）。
include(GNUInstallDirs)

# Linux desktop integration.  Windows uses its embedded icon and the optional
# Explorer scripts instead.
# Linux 桌面集成；Windows 使用嵌入图标与可选的资源管理器脚本。
if(NOT WIN32)
    # Freedesktop desktop entry.
    # Freedesktop 桌面入口文件。
    install(FILES qarchiver.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    # Application icon registered in the hicolor theme.  Install all common
    # nominal sizes plus a pixmaps fallback so panel/dock/launcher lookups hit
    # a crisp bitmap on every desktop environment.
    # 注册到 hicolor 主题的应用图标。安装全部常见尺寸，并提供 pixmaps 兜底，
    # 让面板/任务栏/启动器在各类桌面环境都能找到清晰图标。
    foreach(_size 512 256 128 64 48 32)
        install(FILES
            ${PROJECT_SOURCE_DIR}/resources/app-icons/qarchiver-${_size}.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/${_size}x${_size}/apps
            RENAME qarchiver.png
        )
    endforeach()
    install(FILES ${PROJECT_SOURCE_DIR}/resources/app-icons/qarchiver-256.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/pixmaps
        RENAME qarchiver.png
    )
endif()

# Windows shell integration scripts and platform notes.
# Windows 资源管理器集成脚本与平台说明。
if(WIN32)
    add_subdirectory(windows)
endif()
