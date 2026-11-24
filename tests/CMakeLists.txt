# Test suite: unit and integration tests for the core, backends and UI wiring.
# 娴嬭瘯濂椾欢锛氳鐩栨牳蹇冨眰銆佸悗绔笌 UI 杩炴帴鏂瑰紡鐨勫崟鍏冨強闆嗘垚娴嬭瘯銆?
#
# Every test links one of the layer libraries built by src/ instead of listing
# source files, so adding a source file never requires touching this file.
# 姣忎釜娴嬭瘯閮介摼鎺?src/ 鏋勫缓鐨勫垎灞傞潤鎬佸簱锛岃€屼笉鍐嶇綏鍒楁簮鏂囦欢锛?
# 鍥犳鏂板婧愭枃浠舵棤闇€淇敼鏈枃浠躲€?

# Qt Test is optional; the application itself does not require it.
# Qt Test 鏄彲閫変緷璧栵紱搴旂敤鏈韩涓嶉渶瑕佸畠銆?
find_package(Qt6 ${QARCHIVER_MIN_QT_VERSION} QUIET COMPONENTS Core Gui Widgets Concurrent Test)
if(NOT TARGET Qt6::Test)
    message(STATUS "Qt6::Test not found: skipping QArchiver tests")
    return()
endif()

# Enable moc/automoc for the standalone test executables.
# 涓虹嫭绔嬫祴璇曞彲鎵ц鏂囦欢鍚敤 moc/automoc銆?
set(CMAKE_AUTOMOC ON)

# Helper: declare a Qt test executable and register it with CTest.
# 杈呭姪鍑芥暟锛氬０鏄?Qt 娴嬭瘯鍙墽琛屾枃浠跺苟娉ㄥ唽鍒?CTest銆?
#
#   qarchiver_add_test(tst_name
#       SOURCES  tst_name.cpp
#       LIBS     qarchiver_core          # optional
#       DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test_files   # optional
#   )
function(qarchiver_add_test name)
    cmake_parse_arguments(TEST "" "DATA_DIR" "SOURCES;LIBS" ${ARGN})

    qt_add_executable(${name} ${TEST_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${TEST_LIBS})

    if(TEST_DATA_DIR)
        target_compile_definitions(${name} PRIVATE
            QARCHIVER_TEST_DATA_DIR="${TEST_DATA_DIR}")
    endif()

    add_test(NAME ${name} COMMAND ${name})
endfunction()

# Utility tests: path normalization, traversal checks and -slt parsing.
# 宸ュ叿娴嬭瘯锛氳矾寰勮鑼冨寲銆佽矾寰勭┛瓒婃鏌ヤ笌 -slt 瑙ｆ瀽銆?
qarchiver_add_test(tst_utils
    SOURCES tst_utils.cpp
    LIBS qarchiver_core
)

# Toolchain discovery: --help validation, system fallback and RAR codec probe.
# 宸ュ叿閾惧彂鐜帮細--help 鏍￠獙銆佺郴缁熷洖閫€涓?RAR 缂栬В鐮佸櫒鎺㈡祴銆?
qarchiver_add_test(tst_toolchain
    SOURCES tst_toolchain.cpp
    LIBS qarchiver_utils
)

# Background image helpers: alignment, scaling, target rectangles and painting.
# 鑳屾櫙鍥剧墖杈呭姪鍑芥暟锛氬榻愩€佺缉鏀俱€佺洰鏍囩煩褰笌缁樺埗銆?
qarchiver_add_test(tst_background
    SOURCES tst_background.cpp
    LIBS qarchiver_ui
)
set_tests_properties(tst_background PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

# Backend tests: tar/zip/7z/RAR/stream round-trips and special names.
# 鍚庣娴嬭瘯锛歵ar/zip/7z/RAR/娴佸線杩斿強鐗规畩鏂囦欢鍚嶃€?
qarchiver_add_test(tst_backends
    SOURCES tst_backends.cpp
    LIBS qarchiver_core
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test_files
)

# Continuous image preview: same-directory image selection and batch extraction.
# 杩炵画鐪嬪浘锛氬悓鐩綍鍥剧墖閫夊彇涓庢壒閲忚В鍘嬨€?
qarchiver_add_test(tst_preview
    SOURCES tst_preview.cpp
    LIBS qarchiver_ui
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test_files
)

# ArchiveManager tests: async open/list/extract, fallback and preferences.
# ArchiveManager 娴嬭瘯锛氬紓姝ユ墦寮€/鍒楄〃/瑙ｅ帇銆佸洖閫€涓庡亸濂借缃€?
qarchiver_add_test(tst_manager
    SOURCES tst_manager.cpp
    LIBS qarchiver_ui
    DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test_files
)

# libarchive backend: only built when the system library was found.
# libarchive 鍚庣锛氫粎褰撴壘鍒扮郴缁熷簱鏃舵瀯寤恒€?
if(QARCHIVER_HAS_LIBARCHIVE)
    qarchiver_add_test(tst_libarchive
        SOURCES tst_libarchive.cpp
        LIBS qarchiver_core
    )
endif()

# Drag-out staging (DragOutManager): needs the manager, AppSettings and the
# backends, but no widgets - it runs headless.
# 鎷栧嚭鏆傚瓨锛圖ragOutManager锛夛細闇€瑕佺鐞嗗櫒銆丄ppSettings 鍜屽悗绔紝
# 浣嗕笉渚濊禆鎺т欢锛屽彲鏃犵晫闈㈣繍琛屻€?
qarchiver_add_test(tst_dragout
    SOURCES tst_dragout.cpp
    LIBS qarchiver_ui
)

# Preferences and password dialog tests (offscreen QPA).
# 鍋忓ソ璁剧疆涓庡瘑鐮佸璇濇娴嬭瘯锛堢灞?QPA锛夈€?
qarchiver_add_test(tst_preferences
    SOURCES tst_preferences.cpp
    LIBS qarchiver_ui
)
set_tests_properties(tst_preferences PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen")


# Make the bundled 7-Zip helper visible to every test that spawns a CLI
# backend.  QARCHIVER_7Z_PATH is the first entry in
# ProcessRunner::resolveToolPath(), so tests use exactly the binary that ships
# with QArchiver even when no system 7z is installed.
# 璁╂瘡涓細鍚姩 CLI 鍚庣鐨勬祴璇曢兘鑳芥壘鍒伴殢绋嬪簭鍒嗗彂鐨?7-Zip 杈呭姪绋嬪簭銆?
# QARCHIVER_7Z_PATH 鏄?ProcessRunner::resolveToolPath() 鐨勭涓€浼樺厛绾э紝
# 鍥犳鍗充娇绯荤粺鏈畨瑁?7z锛屾祴璇曚篃浼氫娇鐢?QArchiver 鑷甫鐨勪簩杩涘埗銆?
if(QARCHIVER_BUNDLE_7ZIP)
    set(_qarchiver_test_7z_env "QARCHIVER_7Z_PATH=${QARCHIVER_7ZIP_EXE}")
    foreach(_qarchiver_test IN ITEMS
            tst_utils tst_toolchain tst_background tst_backends tst_preview
            tst_manager tst_libarchive tst_dragout tst_preferences)
        if(TEST ${_qarchiver_test})
            set_tests_properties(${_qarchiver_test} PROPERTIES
                ENVIRONMENT "QT_QPA_PLATFORM=offscreen;${_qarchiver_test_7z_env}")
        endif()
    endforeach()
endif()
