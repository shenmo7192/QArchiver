# QArchiver / 小Q解压

[English](./README_en.md) | 中文

基于 Qt 6 的图形化压缩包管理器：浏览、解压、创建、追加、删除、测试
7z / zip / tar / gz / bz2 / xz / rar 归档。

## 功能

- 浏览归档内容，树形与列表两种视图，支持过滤与排序
- 解压：全部或选中条目、覆盖策略（询问/覆盖/跳过/重命名）、密码、编码提示
- 压缩：7z / zip / tar / tar.gz / tar.bz2 / tar.xz；系统安装 `rar` 时还可创建 RAR 及 RAR 分卷
- 追加与删除条目（7z / zip / tar 系）
- 完整性测试，解压前可选自动测试
- 归档内单文件预览（文本/图片）；图片会连同同目录图片一起预解压，支持系统查看器连续看图。
  同目录图片超过可配置阈值（默认 200 张，0 表示任意批次）时显示可取消的准备对话框；
  该阈值只决定是否提示，不限制批量解压总量
- 拖出：把选中条目直接拖到文件管理器/桌面即完成解压并保留目录结构
  （后台解压到 staging 目录，`Esc` 可取消；同一选中项第二次拖动零延迟）

## 安全与可靠性

- Zip Slip 路径穿越防护；解压先落临时目录再移动，失败立即清理临时内容
- 解压移动阶段若只有部分文件成功，会明确提示“已移动文件未回滚”，不会静默当作完整成功
- 符号链接两层防护：解压前预检并询问“中止/跳过/本任务全部跳过”，移动前复查实际链接；
  目录链接被跳过时连同其子树一起跳过
- libarchive 读取时的硬链接会安全重建；目标缺失、越界或本身是符号链接时中止任务并提示，
  不会静默生成空文件
- 压缩替换原子化：旧压缩包/分卷先备份，新文件全部就位后才替换，失败自动回滚
- 压缩包列表输出流式解析，避免为列表额外保留完整命令行文本；大小累计带溢出检查
- 打开系统查看器或解压目标目录失败时会给出警告，不静默失败

## 界面与个性化

- Frutiger Aero / Windows 7 风格浅色与 Aero Dark 深色主题，中英文界面
- 可选半透明背景图片：九种对齐（含四角/四边/居中）、自定义尺寸与透明度，支持带 alpha 的 PNG；
  占位页显示在中央区域，归档页只显示在内容视图的可视区域内，不覆盖表头与滚动条

## 配置项

背景图片在“偏好设置 → 背景图片”中选择；路径为空即关闭。尺寸留空/0 表示“自动”
（宽高只填一个时保持比例），透明度范围为 0–100%。

其他可配置项（偏好设置）：

- 连续看图准备对话框阈值：`preview/continuousImageDialogThreshold`，默认 200，范围 0–9999；
- 最近文件数量上限：`history/recentFilesLimit`，默认 20，范围 1–100，降低时立即裁剪并刷新侧边栏。

## 依赖

构建期（Debian/Ubuntu 包名，Qt 6.4 起可用）：

| 依赖 | 是否必需 | 说明 |
| --- | --- | --- |
| Qt 6.4+（Core Gui Widgets Concurrent） | 必需 | `qt6-base-dev (>= 6.4)` |
| Qt LinguistTools（lupdate/lrelease） | 必需 | `qt6-tools-dev (>= 6.4)`，翻译在构建时编译 |
| CMake 3.20+、GCC 12+/Clang 15+（C++20） | 必需 | |
| libarchive 3.4+（开发头文件） | 可选（默认启用） | `libarchive-dev`；缺失时仅 WARNING，自动退回纯命令行后端 |

运行期：

| 依赖 | 是否必需 | 说明 |
| --- | --- | --- |
| 7-Zip CLI | 已内置 | `third_party/7zip/`（Linux `7zzs`，Windows `7z.exe + 7z.dll`）随程序分发；负责 7z/zip/tar 读写、纯流解压和 RAR 读取 |
| rar 命令 | 可选 | 系统安装后才会启用 RAR 创建与 RAR 分卷创建；可用 `QARCHIVER_RAR_PATH` 指定路径 |
| libarchive13 | 启用 libarchive 时必需 | 程序加载的是构建时探测到的共享库 |
| qt6-translations-l10n | 可选 | 提供 `qtbase_<lang>.qm`，用于本地化标准对话框按钮与文件对话框；缺失时应用自身文本仍会翻译 |

构建时会把当前架构的 7-Zip 辅助程序复制到可执行文件旁的 `7zip/`
目录（Linux 安装到 `libexec/qarchiver/7zip/`）。运行时优先使用这个内置
版本，只有找不到时才回退到系统 PATH 中的 `7z` / `7zzs`。

启动时会检查兜底工具链：

1. 先在程序相关目录（`<可执行文件目录>/7zip`、`libexec/qarchiver/7zip` 等）
   查找 7-Zip 命令行工具，并用 `--help` 确认它确实是 7-Zip；
2. 找不到内置工具时，再查找系统 PATH 中的 7-Zip，并通过 `7z i` 输出的
   `Codecs:` 区段判断是否支持 RAR 解压；
3. 如果正在使用系统 7-Zip，启动后会弹窗一次，显示所用路径及 RAR 支持情况；
   如果完全找不到，会提示下载官方 7-Zip 命令行工具放到 `7zip/` 目录，或
   在 Debian/Ubuntu 上安装系统包（含 `7zip-rar` / `p7zip-rar`）。
   探测在后台线程执行，不阻塞窗口显示。

`QARCHIVER_7Z_PATH` 可显式指定 7-Zip 可执行文件，绕过上述自动探测。

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build            # 可选，默认装到 /usr/local
```

常用选项：

| 选项 | 默认 | 说明 |
| --- | --- | --- |
| `-DQARCHIVER_USE_LIBARCHIVE=ON|OFF` | `ON` | 关闭后只编译命令行后端，不链接 libarchive |
| `-DCMAKE_BUILD_TYPE=Release|Debug|RelWithDebInfo` | 空 | |
| `-DBUILD_TESTING=OFF` | `ON` | 是否生成 `tests/` |

若 libarchive 只提供 pkg-config 文件而没有 CMake 配置包，构建脚本会自动
回退到 `pkg_check_modules`。

## 读取后端与开关

读取有两条路径：

- **libarchive（默认）**：zip（含 ZipCrypto/AES）、明文 7z、tar / tar.gz /
  tar.bz2 / tar.xz、单文件 gz / bz2 / xz 在进程内解析，不启动外部程序。
  无法处理的情形（加密 7z、7z/zip 分卷、rar）会自动回退到命令行后端，
  并把待执行的操作重放一次。
- **命令行**：7z/zip/tar/gz/bz2/xz 由内置 7-Zip 读写，RAR 读取由内置 7-Zip 完成，
  不再依赖系统 `tar` / `unrar`。若系统安装了 `rar`，RAR 创建与分卷创建会使用该命令。
  7-Zip 调用统一使用 `-sccUTF-8`、`--` 和 `-spd`，选中成员通过 UTF-8 或原始代码页
  listfile 传入，避免通配符、`@`、前导 `-` 和旧 ZIP 编码问题。

写入（压缩、追加、删除、分卷）始终由命令行完成，libarchive 后端只读。

两级开关，均可关闭 libarchive：

1. 设置 → 偏好设置 → “使用 libarchive 读取压缩包”
   （配置键 `backends/useLibArchive`，默认开启）。**更改在下次打开压缩包时生效**，
   不会打断当前已打开归档的会话。
2. 环境变量 `QARCHIVER_NO_LIBARCHIVE=1`：启动即禁用，优先级高于配置项，
   适合对比排查问题。

## 测试

```bash
cd build
ctest --output-on-failure
QT_QPA_PLATFORM=offscreen ctest --output-on-failure   # 无显示环境
```

测试覆盖：工具函数（路径规范化/穿越检测/大小溢出/符号链接目标）、CLI 后端、
管理器（分派、取消、回退、打开失败保留当前包、危险符号链接决策）、
libarchive 后端（往返、AES 密码、损坏检测、选择性解压、硬链接重建）、
偏好对话框与配置同步（连续看图阈值、最近文件上限）、RAR/RAR 分卷创建与
旧分卷替换、兜底 7-Zip 的 `--help` 校验与 RAR Codecs 探测、图片同目录
连续预览、列表模式目录索引、背景图片缓存。测试通过 `QARCHIVER_7Z_PATH` 使用内置
7-Zip；RAR 创建用例在系统没有 `rar` 时会自动跳过，也可用
`QARCHIVER_RAR_PATH` 指定待测的 rar 可执行文件。
`tarDotPrefixAndNonAscii` 兼容性用例在系统没有 `tar` 时会自动跳过。

## 调试与诊断

| 环境变量 | 作用 |
| --- | --- |
| `SAM_DEBUG=1` | 输出 Debug 级日志 |
| `QARCHIVER_NO_LIBARCHIVE=1` | 启动即禁用 libarchive 后端 |
| `QARCHIVER_7Z_PATH` | 显式指定 7-Zip 命令行工具路径，跳过自动探测 |
| `QARCHIVER_RAR_PATH` | 显式指定 rar 命令路径，用于 RAR 创建 |
| `QARCHIVER_DRAG_DEBUG_NO_EXEC=1` | 拖出时只准备 staging 并打印 URL，不进入阻塞的原生拖拽循环 |

拖放相关的排查工具（X11 下可自动复现/对照的拖拽源与日志化接收端）见
`~/.app/ai_tools/dnd-drag-test/`；拖出失效时先看“目标端有没有来请求数据”，
比看 `QDrag::exec()` 的返回值可靠。

## 已知限制

- 加密 7z 无法用 libarchive 读取，会回退到 7z 命令行（仍需密码）
- libarchive 不支持 7z/zip 分卷；分卷归档直接在打开时走命令行
- RAR 读取由内置 7-Zip 完成；创建 RAR/RAR 分卷需要系统安装 `rar`，
  已创建 RAR 的追加/删除仍不支持
- 危险符号链接默认中止解压；用户选择“跳过”时该链接（目录链接则连同子树）不会解压
- 硬链接仅在 libarchive 读后端中安全重建；目标缺失/越界/为符号链接时任务会中止并提示
- 未设置 locale 时，libarchive 无法转换非 UTF-8 条目名；这类归档需在
  解压对话框里指定编码（如 GBK/CP936/GB18030）
- 归档注释的显示/编辑不再提供

## 许可证

AGPL-3.0，见 `LICENSE`。程序运行期调用外部工具、并与 libarchive（BSD-2-Clause）
链接；内置的 7-Zip 按 GNU LGPL（含 unRAR 限制）分发，许可证见
`third_party/7zip/License.txt`，安装后位于 `share/doc/qarchiver/`。
程序不再依赖非自由的 `unrar`；RAR 读取由内置 7-Zip 完成。
RAR 创建是可选的，仅在用户系统已安装 `rar` 命令时启用，程序本身不分发该命令。

## 打包参考

见 `packaging/README.md`。
