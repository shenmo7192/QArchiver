# 打包说明

目前仓库只提供 `qarchiver.desktop`（由 `cmake --install` 安装到
`share/applications`）。本文件列出打包者需要声明的依赖，以及可直接改用的
Debian 控制文件片段。**这些片段未在 CI 中验证过**，合并进正式打包脚本前
请先在目标发行版上跑一遍。

## 运行期依赖

| 能力 | 依赖 | Debian/Ubuntu 包 | 备注 |
| --- | --- | --- | --- |
| 启动 | Qt 6 运行库 | `libqt6core6`, `libqt6gui6`, `libqt6widgets6`, `libqt6concurrent6` | 由 dh_shlibdeps 自动生成，通常不用手写 |
| 读取 zip / 明文 7z / tar 系 / gz|bz2|xz | libarchive | `libarchive13` / `libarchive13t64` | 构建时链接，缺失会启动失败，必须声明 |
| 7z / zip 的创建、追加、删除、分卷 | 7-Zip CLI | `7zip`（Debian 13+）或 `p7zip-full`（≤ Debian 12） | 可执行文件名均为 `7z`；缺失时相关功能不可用 |
| 打开 rar | unrar | `unrar`（non-free / multiverse） | 非自由许可，建议作为 `Recommends` 或 `Suggests` |
| tar 系兜底与格式探测 | tar | `tar` | 基本系统自带 |

注意：

- 若以 `-DQARCHIVER_USE_LIBARCHIVE=OFF` 构建，则不应声明 libarchive 依赖，
  但 7z 从“推荐”变为“必需”。
- 打 AppImage / Flatpak 时同样需要打包 `libarchive` 与 `7z`；unrar 因许可
  问题建议由用户在宿主机自行安装。

## Debian 控制文件片段

```text
Package: qarchiver
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}, 7zip | p7zip-full
Recommends: libarchive13 | libarchive13t64
Suggests: unrar
Description: Qt based archive manager
 小Q解压 is a Qt 6 archive manager for browsing, extracting, creating,
 appending to, deleting from and testing 7z, zip, tar, gz, bz2, xz and rar
 archives. Archives are read in-process through libarchive when possible and
 fall back to the 7z, tar and unrar command line tools otherwise.
```

`${shlibs:Depends}` 会把 `libarchive13` 一起带出来（链接时才需要，因此若
上游构建关闭了 libarchive，就不会误报）。

## 桌面文件

`qarchiver.desktop` 已声明 `MimeType`，安装后需要一次
`update-desktop-database`（debhelper / `desktop-file-utils` 的触发器会处理）。
图标安装为 `share/icons/hicolor/128x128/apps/qarchiver.png`，`Icon=qarchiver`
与之对应。
