# QArchiver / 小Q解压

基于 Qt 6 的图形化压缩包管理器：浏览、解压、创建、追加、删除、测试
7z / zip / tar / gz / bz2 / xz / rar 归档。

## 功能

- 浏览归档内容，树形与列表两种视图，支持过滤与排序
- 解压：全部或选中条目、覆盖策略（询问/覆盖/跳过/重命名）、密码、编码提示
- 压缩：7z / zip / tar / tar.gz / tar.bz2 / tar.xz，可选压缩等级、固实、分卷、加密
- 追加与删除条目（7z / zip / tar 系）
- 完整性测试，解压前可选自动测试
- 归档内单文件预览（文本/图片），后台线程解压，不阻塞界面
- 拖出：把选中条目直接拖到文件管理器/桌面即完成解压并保留目录结构
  （后台解压到 staging 目录，`Esc` 可取消；同一选中项第二次拖动零延迟）
- Zip Slip 路径穿越防护；解压先落临时目录再移动，失败不留半成品
- 深色/浅色主题，中英文界面

## 依赖

构建期（Debian 13 / trixie 包名）：

| 依赖 | 是否必需 | 说明 |
| --- | --- | --- |
| Qt 6.8+（Core Gui Widgets Concurrent） | 必需 | `qt6-base-dev` |
| Qt LinguistTools（lupdate/lrelease） | 必需 | `qt6-tools-dev`，翻译在构建时编译 |
| CMake 3.20+、GCC 12+/Clang 15+（C++20） | 必需 | |
| libarchive 3.4+（开发头文件） | 可选（默认启用） | `libarchive-dev`；缺失时仅 WARNING，自动退回纯命令行后端 |

运行期：

| 依赖 | 是否必需 | 说明 |
| --- | --- | --- |
| 7z（7-Zip 21+ / 7zip 包） | 必需 | 7z/zip 的创建、追加、删除、分卷，tar 系兜底 |
| tar | 可选 | 用于格式探测，以及未启用 libarchive 时的 tar 系读写 |
| unrar | 打开 rar 时必需 | 非自由软件，未安装时打开 rar 会提示缺少工具 |
| libarchive13 | 启用 libarchive 时必需 | 程序加载的是构建时探测到的共享库 |

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
- **命令行**：7z / unrar / tar，等同早期版本的行为。

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

测试覆盖：工具函数（路径规范化/穿越检测/重名）、CLI 后端、管理器（分派、
取消、回退）、libarchive 后端（往返、AES 密码、损坏检测、选择性解压）、
偏好对话框与配置同步。缺少 `7z` 时依赖真实归档的用例会自动 SKIP。

## 调试与诊断

| 环境变量 | 作用 |
| --- | --- |
| `SAM_DEBUG=1` | 输出 Debug 级日志 |
| `QARCHIVER_NO_LIBARCHIVE=1` | 启动即禁用 libarchive 后端 |
| `QARCHIVER_DRAG_DEBUG_NO_EXEC=1` | 拖出时只准备 staging 并打印 URL，不进入阻塞的原生拖拽循环 |

拖放相关的排查工具（X11 下可自动复现/对照的拖拽源与日志化接收端）见
`~/.app/ai_tools/dnd-drag-test/`；拖出失效时先看“目标端有没有来请求数据”，
比看 `QDrag::exec()` 的返回值可靠。

## 已知限制

- 加密 7z 无法用 libarchive 读取，会回退到 7z 命令行（仍需密码）
- libarchive 不支持 7z/zip 分卷；分卷归档直接在打开时走命令行
- rar 始终使用 `unrar`，未通过 libarchive 读取
- 未设置 locale 时，libarchive 无法转换非 UTF-8 条目名；这类归档需在
  解压对话框里指定编码（如 GBK/CP936/GB18030）
- 归档注释的显示/编辑不再提供

## 许可证

AGPL-3.0，见 `LICENSE`。程序运行期调用外部工具、并与 libarchive（BSD-2-Clause）
链接；`unrar` 属非自由许可，仅为可选运行期依赖，不随本项目分发。

## 打包参考

见 `packaging/README.md`。
