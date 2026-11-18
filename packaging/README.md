# 打包说明

目前仓库只提供 `qarchiver.desktop`（由 `cmake --install` 安装到
`share/applications`）。本文件列出打包者需要声明的依赖，以及可直接改用的
Debian 控制文件片段。**这些片段未在 CI 中验证过**，合并进正式打包脚本前
请先在目标发行版上跑一遍。

## 运行期依赖

| 能力 | 依赖 | Debian/Ubuntu 包 | 备注 |
| --- | --- | --- | --- |
| 启动 | Qt 6 运行库 | `libqt6core6`, `libqt6gui6`, `libqt6widgets6`, `libqt6concurrent6` | 由 dh_shlibdeps 自动生成，通常不用手写 |
| 读取 zip / 明文 7z / tar 系 / gz|bz2|xz | libarchive（可选） | `libarchive13` / `libarchive13t64` | 构建时链接，启用时运行期必须声明 |
| 7z / zip / tar 系读写、纯流、RAR 只读 | 内置 7-Zip | 无（随包安装） | 安装到 `/usr/libexec/qarchiver/7zip/7zzs`，不要额外声明 `7zip` / `tar` / `unrar` 依赖 |

注意：

- 若以 `-DQARCHIVER_USE_LIBARCHIVE=OFF` 构建，则不应声明 libarchive 依赖；
  7-Zip 辅助程序仍由 `cmake --install` 自动安装，无需改为系统依赖。
- 打 AppImage / Flatpak 时请确认 `libexec/qarchiver/7zip/7zzs` 被包含进包；
  tar 与 RAR 读取都由该内置程序完成，不需要宿主机安装 `tar` / `unrar`。

## Debian 控制文件片段

```text
Package: qarchiver
Architecture: any
Depends: ${shlibs:Depends}, ${misc:Depends}
Recommends: libarchive13 | libarchive13t64
Description: Qt based archive manager
 小Q解压 is a Qt 6 archive manager for browsing, extracting, creating,
 appending to, deleting from and testing 7z, zip, tar, gz, bz2, xz and rar
 archives. Archives are read in-process through libarchive when possible and
 fall back to the bundled 7-Zip helper (which also handles tar write and
 RAR read) otherwise.
```

`${shlibs:Depends}` 会把 `libarchive13` 一起带出来（链接时才需要，因此若
上游构建关闭了 libarchive，就不会误报）。

## 桌面文件

`qarchiver.desktop` 已声明 `MimeType`，安装后需要一次
`update-desktop-database`（debhelper / `desktop-file-utils` 的触发器会处理）。
图标安装为 `share/icons/hicolor/128x128/apps/qarchiver.png`，`Icon=qarchiver`
与之对应。
