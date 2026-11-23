# Optional Windows Explorer integration scripts shipped in the ZIP package.
# 随 Windows ZIP 包分发的可选资源管理器集成脚本。

if(WIN32)
    install(FILES
        install-shell-menu.ps1
        uninstall-shell-menu.ps1
        README.md
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
