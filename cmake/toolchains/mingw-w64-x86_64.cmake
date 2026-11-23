# Cross-compilation toolchain for Windows x86-64 with Debian's MinGW-w64.
# Debian MinGW-w64 交叉编译 Windows x86-64 的工具链文件。
#
# Usage / 用法:
#   cmake -S . -B build-windows \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
#     -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.2/mingw_64 \
#     -DQT_HOST_PATH=/path/to/Qt/6.8.2/gcc_64 \
#     -DQARCHIVER_USE_LIBARCHIVE=OFF \
#     -DQARCHIVER_WINDOWS_DEPLOY=OFF \
#     -DBUILD_TESTING=OFF

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
