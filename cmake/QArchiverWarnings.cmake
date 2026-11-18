# Compiler warning policy shared by all QArchiver targets.
# 所有 QArchiver 目标共用的编译器警告策略。
function(qarchiver_set_warnings target)
    if(MSVC)
        # MSVC: high warning level with standard conformance.
        # MSVC：高警告等级并启用标准一致性检查。
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        # GCC/Clang: broad warnings; unused parameters are common in Qt slots.
        # GCC/Clang：启用较全面的警告；Qt 槽函数中未使用参数较常见。
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        )
    endif()
endfunction()
