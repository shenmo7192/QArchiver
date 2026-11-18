# Desktop integration files installed with QArchiver.
# 随 QArchiver 安装的桌面集成文件。

# Standard install destinations (bin/lib/share).
# 标准安装目录（bin/lib/share）。
include(GNUInstallDirs)

# Freedesktop desktop entry.
# Freedesktop 桌面入口文件。
install(FILES qarchiver.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

# Application icon registered in the hicolor theme.
# 注册到 hicolor 主题的应用图标。
install(FILES ${PROJECT_SOURCE_DIR}/src/resources/icons/QArchiver.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/128x128/apps
    RENAME qarchiver.png
)
