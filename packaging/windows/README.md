# Windows 构建与使用 / Windows build and usage

QArchiver 的 Windows 版本使用与 Linux 相同的 Qt 6 界面，并额外提供：

- 原生的 exe 图标、版本信息与 Per-Monitor V2 DPI 清单；
- 支持 Windows x64 与 ARM64，并分别内置原生架构的 7-Zip；
- 启动 7z/rar 时不弹出控制台窗口；
- Windows 文件名代码页（GBK/Big5/Shift-JIS）转换，不依赖 iconv；
- 安装器（开始菜单、可选桌面快捷方式、卸载入口）与便携 ZIP 两种分发形式；
- 资源管理器右键菜单：“用 小Q解压 打开”“一键解压”，文件和文件夹的
  “添加到同名 ZIP 压缩包”与“压缩到...”二级菜单（7z/ZIP/tar.gz/tar.xz/
  tar.bz2/tar，另含“自定义压缩选项”）。

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

生成 `QArchiver-2.3.0-windows-x64.zip`，根目录包含 `QArchiver.exe`、
`7zip\7z.exe`、`7zip\7z.dll` 以及 `windeployqt` 部署的 Qt 运行库。

在便携 ZIP 基础上生成安装器（需要 Inno Setup 6 的 `ISCC.exe`）：

```powershell
.\build-installer.ps1 -ZipPath .\QArchiver-2.3.0-windows-x64.zip -Arch x64
```

生成 `QArchiver-2.3.0-windows-x64-setup.exe`。安装器默认安装到
`Program Files\QArchiver`，创建开始菜单项（桌面快捷方式为可选项）、卸载入口，
并询问是否注册资源管理器右键菜单与关联压缩包格式（默认选中）。安装器运行时
会注册 `QArchiverShell.dll`（进程内右键菜单扩展）；关联格式由
`QArchiver.exe --register-associations` 为当前用户写入 ProgID。

ARM64 在 x64 主机上交叉构建（Qt 包使用
`win64_msvc2022_arm64_cross_compiled`）：

```powershell
cmake -S . -B build-arm64 -G "Visual Studio 17 2022" -A ARM64 ^
      -DCMAKE_PREFIX_PATH=C:\Qt\6.8.2\msvc2022_arm64_cross_compiled ^
      -DQARCHIVER_USE_LIBARCHIVE=OFF -DBUILD_TESTING=OFF
cmake --build build-arm64 --config Release
cpack --config build-arm64/CPackConfig.cmake -C Release
```

生成 `QArchiver-2.3.0-windows-arm64.zip`，内置原生 ARM64 的
`7zip\7z.exe` / `7z.dll`。ARM64 可执行文件无法在 x64 主机上运行测试，
因此不执行 `ctest`。

生成 ARM64 安装器：

```powershell
.\build-installer.ps1 -ZipPath .\QArchiver-2.3.0-windows-arm64.zip -Arch arm64
```

MinGW 构建时请使用与 Qt 包匹配的编译器，并把 `libstdc++-6.dll`、
`libgcc_s_seh-1.dll`、`libwinpthread-1.dll` 放到 exe 旁；CMake 的安装规则
会在 `MINGW` 下自动尝试复制这些 DLL。

## 右键菜单

解压便携 ZIP 后，在 PowerShell 中运行（当前用户，无需管理员）：

```powershell
powershell -ExecutionPolicy Bypass -File .\install-shell-menu.ps1
```

安装所有用户（需要管理员）：

```powershell
powershell -ExecutionPolicy Bypass -File .\install-shell-menu.ps1 -Scope Machine
```

卸载：

```powershell
powershell -ExecutionPolicy Bypass -File .\uninstall-shell-menu.ps1
```

右键菜单内容（由 `QArchiverShell.dll` 动态生成）：

- 压缩包：“用 小Q解压 打开”“一键解压”；
- 文件与文件夹：动态文案“添加到 xx.zip”、“压缩到...”二级菜单
  （7z/ZIP/tar.gz/tar.xz/tar.bz2/tar）和“自定义压缩选项”；
  已经是压缩包的文件不显示这些压缩命令；
- 文件夹空白处：压缩当前文件夹。

文件关联可在“偏好设置 → 工具 → 文件关联”中管理（Windows / Linux）。

“一键解压”等价于：

```powershell
.\QArchiver.exe --oneclick "D:\path\archive.zip"
```

“添加到同名 ZIP”等价于（在源文件旁生成 `名称.zip`）：

```powershell
.\QArchiver.exe --compress zip "D:\path\name.txt"
```

它使用“偏好设置 → 一键解压”中的目标位置和“解压完成后关闭”设置。

## 命令行

```powershell
.\QArchiver.exe "D:\path\archive.zip"       # 打开
.\QArchiver.exe --oneclick ".\archive.7z"   # 一键解压
.\QArchiver.exe --compress zip ".\file"     # 直接创建 file.zip
.\QArchiver.exe --compress 7z ".\folder"    # 直接创建 folder.7z
.\QArchiver.exe --compress tar.gz ".\dir" --output .\dir.tar.gz
.\QArchiver.exe --compress-dialog ".\file"  # 打开压缩对话框
.\QArchiver.exe --preferences               # 直接打开设置
.\QArchiver.exe --benchmark                 # 直接打开 7-Zip 性能测试
.\QArchiver.exe --lang en                   # 指定界面语言
```

## GitHub Actions

- `.github/workflows/build.yml` 构建 Linux amd64/arm64 deb、Windows x64/ARM64
  便携 ZIP 与对应安装器；
- `.github/workflows/ci.yml` 在 push / PR 上运行；
- `.github/workflows/release.yml` 在推送 `v*` 标签时创建两个 Release：
  `vX.Y.Z`（安装版）与 `vX.Y.Z-portable`（便携版），各自上传对应产物与
  `SHA256SUMS`。

## 卸载构建依赖（Debian 交叉编译环境）

在 Debian/Ubuntu 上做交叉编译验证时，可用仓库中的
`tools/uninstall-build-deps.sh` 清理 MinGW 与下载的交叉 Qt：

```bash
tools/uninstall-build-deps.sh --dry-run
tools/uninstall-build-deps.sh --yes
```
