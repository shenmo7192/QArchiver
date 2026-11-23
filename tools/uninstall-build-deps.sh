#!/usr/bin/env bash
# QArchiver build-dependency uninstaller.
# QArchiver 构建依赖一键卸载脚本。
#
# This removes only the packages and downloaded toolchains that were installed
# for QArchiver development in this environment:
#   * the mk-build-deps dummy package;
#   * the Debian MinGW-w64 cross compiler packages;
#   * optionally the Qt build tooling (qt6-tools-dev / libarchive-dev);
#   * the aqtinstall host+Windows Qt trees under
#     ~/.local/share/qarchiver-cross and the build-windows directory.
#
# It does not touch your normal Qt runtime, the application itself, your source
# tree or your user settings.
#
# 仅卸载为 QArchiver 开发安装的软件包与下载的工具链，不会影响普通 Qt 运行库、
# 已安装的应用、源码目录或用户设置。
#
# Usage / 用法:
#   tools/uninstall-build-deps.sh                 # interactive plan + confirm
#   tools/uninstall-build-deps.sh --yes           # non-interactive
#   tools/uninstall-build-deps.sh --yes --with-build-deps
#   tools/uninstall-build-deps.sh --dry-run       # print only
#
# Options / 选项:
#   --yes                 Do not ask for confirmation / 不再询问
#   --with-build-deps     Also remove qt6-tools-dev and libarchive-dev
#                         （同时移除 Qt 构建工具与 libarchive 开发包）
#   --autoremove          Run apt autoremove afterwards (may remove other
#                         unused packages) / 之后运行 apt autoremove
#   --keep-cross-toolchain Do not delete ~/.local/share/qarchiver-cross
#   --dry-run             Print the plan and exit / 只打印计划

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "$SCRIPT_DIR/.." && pwd)"
CROSS_DIR="${HOME}/.local/share/qarchiver-cross"

ASSUME_YES=0
WITH_BUILD_DEPS=0
AUTOREMOVE=0
KEEP_CROSS=0
DRY_RUN=0

for arg in "$@"; do
    case "$arg" in
        --yes|-y) ASSUME_YES=1 ;;
        --with-build-deps) WITH_BUILD_DEPS=1 ;;
        --autoremove) AUTOREMOVE=1 ;;
        --keep-cross-toolchain) KEEP_CROSS=1 ;;
        --dry-run) DRY_RUN=1 ;;
        --help|-h)
            sed -n '2,40p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg" >&2
            exit 2
            ;;
    esac
done

PACKAGES=(
    qarchiver-build-deps
    g++-mingw-w64-x86-64
    g++-mingw-w64-x86-64-posix
    g++-mingw-w64-x86-64-win32
    gcc-mingw-w64-x86-64-posix
    gcc-mingw-w64-x86-64-win32
    gcc-mingw-w64-x86-64-posix-runtime
    gcc-mingw-w64-x86-64-win32-runtime
    gcc-mingw-w64-base
    binutils-mingw-w64-x86-64
    mingw-w64-common
    mingw-w64-x86-64-dev
)

if [[ "$WITH_BUILD_DEPS" -eq 1 ]]; then
    PACKAGES+=(qt6-tools-dev libarchive-dev)
fi

INSTALLED=()
for pkg in "${PACKAGES[@]}"; do
    if dpkg-query -W -f='${Status}' "$pkg" 2>/dev/null | grep -q "install ok installed"; then
        INSTALLED+=("$pkg")
    fi
done

echo "QArchiver build-dependency uninstall plan"
echo "仓库 / repository: $REPO_ROOT"
echo
if [[ ${#INSTALLED[@]} -gt 0 ]]; then
    echo "将卸载以下软件包 / packages to remove:"
    printf '  - %s\n' "${INSTALLED[@]}"
else
    echo "没有需要卸载的软件包 / no matching packages installed."
fi

if [[ "$KEEP_CROSS" -eq 0 ]]; then
    echo
    echo "将删除下载的交叉编译 Qt / downloaded cross toolchain:"
    echo "  - $CROSS_DIR"
fi

if [[ -d "$REPO_ROOT/build-windows" ]]; then
    echo "  - $REPO_ROOT/build-windows"
fi
if [[ -d "$REPO_ROOT/build-windows-host-qt" ]]; then
    echo "  - $REPO_ROOT/build-windows-host-qt"
fi
if [[ -e "$REPO_ROOT/.build-windows.lock" ]]; then
    echo "  - $REPO_ROOT/.build-windows.lock"
fi
if [[ "$AUTOREMOVE" -eq 1 ]]; then
    echo
    echo "之后会运行 apt autoremove（可能移除其他不再使用的包）。"
    echo "apt autoremove will run and may remove other unused packages."
fi
echo

if [[ "$DRY_RUN" -eq 1 ]]; then
    echo "Dry run only; nothing was changed. / 仅演示，未做任何修改。"
    exit 0
fi

if [[ "$ASSUME_YES" -ne 1 ]]; then
    read -r -p "Continue? / 继续？[y/N] " answer
    case "$answer" in
        y|Y|yes|YES) ;;
        *) echo "Cancelled. / 已取消。"; exit 0 ;;
    esac
fi

APT_TOOL="apt-get"
if command -v aptss >/dev/null 2>&1; then
    APT_TOOL="aptss"
fi

SUDO=""
if [[ "$(id -u)" -ne 0 ]]; then
    SUDO="sudo"
fi

if [[ ${#INSTALLED[@]} -gt 0 ]]; then
    echo "==> 卸载软件包 / removing packages"
    $SUDO "$APT_TOOL" remove -y "${INSTALLED[@]}"
fi

if [[ "$AUTOREMOVE" -eq 1 ]]; then
    echo "==> apt autoremove"
    $SUDO "$APT_TOOL" autoremove -y
fi

if [[ "$KEEP_CROSS" -eq 0 && -n "$CROSS_DIR" \
      && "$CROSS_DIR" == "$HOME/.local/share/qarchiver-cross" ]]; then
    echo "==> 删除交叉编译 Qt / removing cross toolchain"
    rm -rf -- "$CROSS_DIR"
fi

if [[ -d "$REPO_ROOT/build-windows" ]]; then
    echo "==> 删除 Windows 构建目录 / removing build-windows"
    rm -rf -- "$REPO_ROOT/build-windows"
fi
if [[ -d "$REPO_ROOT/build-windows-host-qt" ]]; then
    echo "==> 删除主机 Qt 前缀 / removing host Qt prefix"
    rm -rf -- "$REPO_ROOT/build-windows-host-qt"
fi
rm -f -- "$REPO_ROOT/.build-windows.lock"

echo
echo "完成。可用 df -h 查看释放的空间。"
echo "Done. Use 'df -h' to see the freed space."
