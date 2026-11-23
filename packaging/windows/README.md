# Windows 构建与使用 / Windows build and usage

QArchiver 的 Windows 版本使用与 Linux 相同的 Qt 6 界面，并额外提供：

- 原生的 exe 图标、版本信息与 Per-Monitor V2 DPI 清单；
- 支持 Windows x64 与 ARM64，并分别内置原生架构的 7-Zip；
- 启动 7z/rar 时不弹出控制台窗口；
- Windows 文件名代码页（GBK/Big5/Shift-JIS）转换，不依赖 iconv；
- 可选的资源管理器右键菜单：“用 小Q解压 打开”和“一键解压”。

## 从源码构建

推荐使用 Qt 6.8 + MSVC 2022。x64 原生构建：

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
      -DCMAKE_PREFIX_PATH=C:\Qt\6.8.2\msvc2022_64 ^
      -DQARCHIVER_USE_LIBARCHIVE=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cpack --config build/CPackConfig.cmake -C Release
```

生成 `QArchiver-2.1.1-windows-x64.zip`，根目录包含 `QArchiver.exe`、
`7zip\7z.exe`、`7zip\7z.dll` 以及 `windeployqt` 部署的 Qt 运行库。

ARM64 在 x64 主机上交叉构建（Qt 包使用
`win64_msvc2022_arm64_cross_compiled`）：

```powershell
cmake -S . -B build-arm64 -G "Visual Studio 17 2022" -A ARM64 ^
      -DCMAKE_PREFIX_PATH=C:\Qt\6.8.2\msvc2022_arm64_cross_compiled ^
      -DQARCHIVER_USE_LIBARCHIVE=OFF -DBUILD_TESTING=OFF
cmake --build build-arm64 --config Release
cpack --config build-arm64/CPackConfig.cmake -C Release
```

生成 `QArchiver-2.1.1-windows-arm64.zip`，内置原生 ARM64 的
`7zip\7z.exe` / `7z.dll`。ARM64 可执行文件无法在 x64 主机上运行测试，
因此不执行 `ctest`。

MinGW 构建时请使用与 Qt 包匹配的编译器，并把 `libstdc++-6.dll`、
`libgcc_s_seh-1.dll`、`libwinpthread-1.dll` 放到 exe 旁；CMake 的安装规则
会在 `MINGW` 下自动尝试复制这些 DLL。

## 右键菜单

解压 ZIP 后，在 PowerShell 中运行（当前用户，无需管理员）：

```powershell
powershell -ExecutionPolicy Bypass -File .\install-shell-menu.ps1
```

卸载：

```powershell
powershell -ExecutionPolicy Bypass -File .\uninstall-shell-menu.ps1
```

右键菜单的“一键解压”等价于：

```powershell
.\QArchiver.exe --oneclick "D:\path\archive.zip"
```

它使用“偏好设置 → 一键解压”中的目标位置和“解压完成后关闭”设置。

## 命令行

```powershell
.\QArchiver.exe "D:\path\archive.zip"       # 打开
.\QArchiver.exe --oneclick ".\archive.7z"   # 一键解压
.\QArchiver.exe --preferences               # 直接打开设置
.\QArchiver.exe --benchmark                 # 直接打开 7-Zip 性能测试
.\QArchiver.exe --lang en                   # 指定界面语言
```

## GitHub Actions

- `.github/workflows/build.yml` 构建 Linux amd64/arm64 deb、Windows x64 zip
  与 Windows ARM64 zip；
- `.github/workflows/ci.yml` 在 push / PR 上运行；
- `.github/workflows/release.yml` 在推送 `v*` 标签时创建 Release 并上传
  四个产物与 `SHA256SUMS`。

## 卸载构建依赖（Debian 交叉编译环境）

在 Debian/Ubuntu 上做交叉编译验证时，可用仓库中的
`tools/uninstall-build-deps.sh` 清理 MinGW 与下载的交叉 Qt：

```bash
tools/uninstall-build-deps.sh --dry-run
tools/uninstall-build-deps.sh --yes
```
