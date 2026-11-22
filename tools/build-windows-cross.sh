#!/usr/bin/env bash
# Cross-compile QArchiver for Windows x86-64 from Debian/Ubuntu.
# 在 Debian/Ubuntu 上为 Windows x86-64 交叉编译 QArchiver。
#
# Prerequisites / 依赖:
#   sudo apt install g++-mingw-w64-x86-64 ninja-build
#   A Windows Qt 6.8MinGW tree (QT_WIN) and a matching host Qt (QT_HOST).
#   Windows Qt 6.8 MinGW 树与版本匹配的主机 Qt。
#
# On an arm64 Debian host without an official arm64 Qt package, the script can
# build a small host prefix from the system Qt:
#   QT_HOST=debian tools/build-windows-cross.sh
# 在无官方 arm64 Qt 包的主机上，可用系统 Qt 生成主机前缀。
#
# Environment / 环境变量:
#   QT_WIN   Windows MinGW Qt prefix, e.g. .../6.8.2/mingw_64
#   QT_HOST  Host Qt prefix, e.g. .../6.8.2/gcc_64, or "debian" for system Qt
#   JOBS     Parallel build jobs (default: nproc)
#
# Output / 输出:
#   build-windows/src/QArchiver.exe
#   build-windows/dist/  (portable folder with Qt DLLs, plugins and 7-Zip)

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$REPO_ROOT/build-windows"
DIST_DIR="$BUILD_DIR/dist"

QT_WIN="${QT_WIN:-${QARCHIVER_WINDOWS_QT:-}}"
QT_HOST="${QT_HOST:-${QARCHIVER_HOST_QT:-}}"
JOBS="${JOBS:-$(nproc)}"

# Prevent two concurrent runs from deleting each other's build tree.
# 防止两个并发构建互相删除对方的构建目录。
LOCK_FILE="$REPO_ROOT/.build-windows.lock"
exec 9>"$LOCK_FILE"
if ! flock -n 9; then
    echo "Another build-windows-cross.sh run is in progress." >&2
    exit 1
fi

if [[ -z "$QT_WIN" || ! -d "$QT_WIN" ]]; then
    cat >&2 <<'MSG'
QT_WIN is required: point it at a Windows MinGW Qt 6.8 prefix.
Example:
  QT_WIN=$HOME/Qt/6.8.2/mingw_64 QT_HOST=$HOME/Qt/6.8.2/gcc_64 \
      tools/build-windows-cross.sh
MSG
    exit 2
fi

if ! command -v x86_64-w64-mingw32-g++ >/dev/null 2>&1; then
    echo "MinGW-w64 is missing: sudo apt install g++-mingw-w64-x86-64" >&2
    exit 2
fi

HOST_CMAKE_DIR=""
if [[ "$QT_HOST" == "debian" ]]; then
    # Debian installs Qt into a multiarch directory; give Qt's cross-compile
    # logic the conventional <prefix>/lib/cmake layout it expects.
    # Debian 将 Qt 安装到 multiarch 目录；构造 Qt 交叉编译逻辑期望的
    # <prefix>/lib/cmake 布局。
    QT_HOST="$REPO_ROOT/build-windows-host-qt"
    rm -rf "$QT_HOST"
    mkdir -p "$QT_HOST"
    ln -s /usr/lib/aarch64-linux-gnu "$QT_HOST/lib"
    ln -s /usr/lib/qt6/bin "$QT_HOST/bin"
    ln -s /usr/lib/qt6/libexec "$QT_HOST/libexec"
    ln -s /usr/lib/qt6/mkspecs "$QT_HOST/mkspecs"
    ln -s /usr/lib/aarch64-linux-gnu/qt6/plugins "$QT_HOST/plugins"
    ln -s /usr/share/qt6/translations "$QT_HOST/translations"
    HOST_CMAKE_DIR="$QT_HOST/lib/cmake"
elif [[ -n "$QT_HOST" ]]; then
    if [[ -d "$QT_HOST/lib/cmake" ]]; then
        HOST_CMAKE_DIR="$QT_HOST/lib/cmake"
    elif [[ -d "$QT_HOST/lib64/cmake" ]]; then
        HOST_CMAKE_DIR="$QT_HOST/lib64/cmake"
    fi
fi

if [[ -z "$QT_HOST" || ! -d "$QT_HOST" ]]; then
    echo "QT_HOST is required (host Qt or 'debian')." >&2
    exit 2
fi

echo "==> Configure / 配置"
rm -rf "$BUILD_DIR"
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$REPO_ROOT/cmake/toolchains/mingw-w64-x86_64.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_WIN" \
    -DQT_HOST_PATH="$QT_HOST" \
    -DQT_HOST_PATH_CMAKE_DIR="$HOST_CMAKE_DIR" \
    -DCMAKE_DISABLE_FIND_PACKAGE_Vulkan=TRUE \
    -DQARCHIVER_USE_LIBARCHIVE=OFF \
    -DQARCHIVER_WINDOWS_DEPLOY=OFF \
    -DBUILD_TESTING=OFF

echo "==> Build / 构建"
cmake --build "$BUILD_DIR" -j"$JOBS"

echo "==> Stage portable folder / 整理便携目录"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/7zip" "$DIST_DIR/plugins/platforms" \
         "$DIST_DIR/plugins/imageformats" "$DIST_DIR/plugins/styles" \
         "$DIST_DIR/plugins/iconengines"

cp "$BUILD_DIR/src/QArchiver.exe" "$DIST_DIR/"
cp "$BUILD_DIR/src/7zip/7z.exe" "$BUILD_DIR/src/7zip/7z.dll" "$DIST_DIR/7zip/"

for dll in Qt6Core.dll Qt6Gui.dll Qt6Widgets.dll Qt6Concurrent.dll; do
    if [[ -f "$QT_WIN/bin/$dll" ]]; then
        cp "$QT_WIN/bin/$dll" "$DIST_DIR/"
    fi
done

for plugin in platforms/qwindows.dll imageformats/qico.dll \
              imageformats/qjpeg.dll imageformats/qgif.dll; do
    if [[ -f "$QT_WIN/plugins/$plugin" ]]; then
        cp "$QT_WIN/plugins/$plugin" "$DIST_DIR/plugins/$plugin"
    fi
done
for style in "$QT_WIN"/plugins/styles/*.dll; do
    [[ -f "$style" ]] && cp "$style" "$DIST_DIR/plugins/styles/"
done
for iconengine in "$QT_WIN"/plugins/iconengines/*.dll; do
    [[ -f "$iconengine" ]] && cp "$iconengine" "$DIST_DIR/plugins/iconengines/"
done

for dll_name in libstdc++-6.dll libgcc_s_seh-1.dll; do
    dll="$(x86_64-w64-mingw32-g++ -print-file-name="$dll_name")"
    if [[ -f "$dll" ]]; then
        cp "$dll" "$DIST_DIR/"
    fi
done
WINPTHREAD="$(x86_64-w64-mingw32-g++ -print-file-name=libwinpthread-1.dll)"
if [[ -f "$WINPTHREAD" ]]; then
    cp "$WINPTHREAD" "$DIST_DIR/"
fi

cat > "$DIST_DIR/qt.conf" <<'QT_CONF'
[Paths]
Prefix=.
Plugins=plugins
Translations=translations
QT_CONF

cp "$REPO_ROOT/packaging/windows/install-shell-menu.ps1" "$DIST_DIR/"
cp "$REPO_ROOT/packaging/windows/uninstall-shell-menu.ps1" "$DIST_DIR/"
cp "$REPO_ROOT/packaging/windows/README.md" "$DIST_DIR/"
cp "$REPO_ROOT/README.md" "$REPO_ROOT/CHANGELOG.md" "$REPO_ROOT/LICENSE" \
   "$REPO_ROOT/third_party/7zip/License.txt" "$DIST_DIR/"
if [[ -d "$QT_WIN/LICENSES" ]]; then
    cp -r "$QT_WIN/LICENSES" "$DIST_DIR/Qt-LICENSES"
fi

echo
echo "Windows package staged at: $DIST_DIR"
echo "Copy it to a Windows x64 machine (or run QArchiver.exe with Wine)."
echo "已整理到 $DIST_DIR；可复制到 Windows x64 机器运行。"
