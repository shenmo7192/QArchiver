# Translations are compiled from .ts into .qm by the Linguist tools from
# qt6-tools-dev (lupdate/lrelease) and embedded into the executable as a
# generated resource.
# 翻译由 qt6-tools-dev 提供的 Linguist 工具（lupdate/lrelease）
# 从 .ts 编译为 .qm，并作为生成资源嵌入可执行文件。
#
# qt_add_translations() places the generated catalogs differently depending on
# the Qt version (Qt 6.4: :/<lang>.qm, newer Qt: :/translations/<lang>.qm).
# Calling qt_add_lrelease() + qt_add_resources() ourselves keeps the resource
# alias fixed, so the runtime path is always :/translations/<lang>.qm and the
# C++ code does not need any Qt-version-specific lookup.
# qt_add_translations() 生成的资源位置随 Qt 版本不同
# （Qt 6.4：:/<lang>.qm，新版 Qt：:/translations/<lang>.qm）。
# 这里显式调用 qt_add_lrelease() + qt_add_resources() 固定资源别名，
# 使运行时路径始终为 :/translations/<lang>.qm，C++ 代码无需按 Qt 版本查找。
# No .qm files are checked in or installed.
# 不提交或安装任何 .qm 文件。

# LinguistTools is required because translations are built into the executable.
# 翻译会被构建进可执行文件，因此必须提供 LinguistTools。
find_package(Qt6 ${QARCHIVER_MIN_QT_VERSION} QUIET COMPONENTS Core LinguistTools)
if(NOT Qt6LinguistTools_FOUND)
    message(FATAL_ERROR
        "Qt6LinguistTools not found: install qt6-tools-dev (or the equivalent "
        "package providing lupdate/lrelease) so the .qm translations can be "
        "built and embedded. / 未找到 Qt6LinguistTools：请安装 qt6-tools-dev "
        "（或提供 lupdate/lrelease 的等效软件包），以便构建并嵌入 .qm 翻译。")
endif()

set(_qarchiver_ts_files
    "${CMAKE_CURRENT_SOURCE_DIR}/zh_CN.ts"
    "${CMAKE_CURRENT_SOURCE_DIR}/en.ts"
)

# Keep the lupdate integration (update_translations target).
# 保留 lupdate 集成（update_translations 目标）。
qt_add_lupdate(QArchiver
    TS_FILES ${_qarchiver_ts_files}
    OPTIONS -no-obsolete
)

# Compile .ts -> .qm; the output list is consumed by qt_add_resources() below.
# 编译 .ts -> .qm；生成结果交给下面的 qt_add_resources() 使用。
qt_add_lrelease(QArchiver
    TS_FILES ${_qarchiver_ts_files}
    QM_FILES_OUTPUT_VARIABLE _qarchiver_qm_files
)

# BASE is the translations build directory, so each alias is just "<lang>.qm"
# and the resulting paths are exactly :/translations/zh_CN.qm and
# :/translations/en.qm on every supported Qt version.
# BASE 为翻译构建目录，因此别名为 "<lang>.qm"，在所有支持的 Qt 版本上
# 生成的路径都恰好是 :/translations/zh_CN.qm 与 :/translations/en.qm。
qt_add_resources(QArchiver "QArchiver_translations"
    PREFIX "/translations"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${_qarchiver_qm_files}
)
