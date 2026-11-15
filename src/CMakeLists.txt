qt_add_executable(QArchiver
    main.cpp
)

target_compile_definitions(QArchiver PRIVATE
    QARCHIVER_VERSION="${PROJECT_VERSION}"
)

target_include_directories(QArchiver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

target_link_libraries(QArchiver PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Concurrent
)

if(TARGET Qt6::DBus)
    target_link_libraries(QArchiver PRIVATE Qt6::DBus)
    target_compile_definitions(QArchiver PRIVATE QARCHIVER_HAS_DBUS)
endif()

qarchiver_set_warnings(QArchiver)

add_subdirectory(core)
add_subdirectory(backends)
add_subdirectory(ui)
add_subdirectory(utils)
add_subdirectory(resources)

install(TARGETS QArchiver
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
