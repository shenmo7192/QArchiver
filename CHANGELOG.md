# Changelog

All notable changes to QArchiver (小Q解压) are documented here.
格式参考 Keep a Changelog；版本号遵循 Semantic Versioning。

## [0.1.2] - 2026-11-23

作者：shenmo <shenmo@spark-app.store>

### Added / 新增

- `resources/svg/modern/`：现代图标的 SVG 源文件，便于重新生成与审阅。
- `tools/update_qrc.py`：按目录结构生成 `icons.qrc`、`themes-aero.qrc`、
  `themes-modern.qrc`。
- `tools/check_resources.py`：校验 qrc 清单、资源前缀与代码中的资源字面量。
- `update_translations` 现在直接调用 `lupdate` 扫描整个 `src/`（此前经
  `qt_add_lupdate()` 只扫描可执行目标，会把所有界面字符串判为过时并删除）。
- `docs/design.zh_CN.md`：按新结构重写的设计文档（根目录的
  `项目设计文档.md` 改为指向它的索引）。
- 现代好压（HaoZip）风格默认界面：扁平渐变头部、大号彩色工具栏、
  文件夹面板、详细信息面板、路径导航与包内搜索、归档状态摘要。
- 现代起始页：拖放区、打开/一键解压入口与最近打开列表。
- 归档内容新增文件管理器式网格视图，并设为默认；导航栏在内容列表上方
  提供网格/列表/树形切换，偏好设置中也可切换。
- 更多常见文件类型图标：字体、磁盘映像、电子书、数据库、种子、设计稿、
  字幕、3D/CAD、安装包、演示文稿等；网格使用 64px 源图保证清晰。
- 压缩格式新增 `.gz`、`.bz2`、`.xz` 单文件压缩流，创建对话框会校验单文件限制。
- 文件选择改用系统原生对话框；压缩源先选择“文件”或“文件夹”，再调用系统对话框。
- 现代外观增加清晰的分区边界：菜单栏/头部/导航/文件夹/内容/详情/状态栏
  使用不同底色与边框；设置、关于按钮与左侧工具按钮统一为大按钮。
- Linux 安装图标提供 32/48/64/128/256/512 多尺寸和 pixmaps 兜底，
  desktop 文件增加 `StartupWMClass`，并通过 `setDesktopFileName` 关联窗口。
- 一键解压首次提示中“解压后关闭”使用强调色并作为默认按钮。
- 现代导航栏的网格/列表/树形切换图标重新绘制并居中；网格视图的文件夹图标居中。
- 网格视图文件名改用多行排版，长文件名可在单元格内完整换行显示。
- 修复并行全新构建中翻译 qrc 可能先于 lrelease 执行、导致嵌入 0 字节 .qm 的问题。
- 一键解压：自动解压到以压缩包命名的独立文件夹，目录冲突时使用
  `名称 (1)` 唯一名称；支持首次提示和“解压完成后关闭程序”。
- 设置中的 7-Zip 性能测试（`7z b`），实时输出、线程数选择、停止与复制结果；
  同时提供“工具 → 7-Zip 性能测试”入口。
- 界面布局切换：现代布局与原有经典布局可在“偏好设置 → 界面”中即时切换。
- Windows 支持：
  - x64 与 ARM64 的 MinGW/MSVC 构建、`.rc` 图标与版本资源、
    Per-Monitor V2 DPI 清单；
  - Win32 文件名编码转换（不再依赖 iconv）；
  - 启动 7z/rar 时隐藏控制台窗口；
  - 跨平台进程存活检测与预览/拖出缓存清理；
  - `windeployqt` 部署规则与 CPack ZIP 打包；ARM64 包内置原生 ARM64 的
    7-Zip 辅助程序（`third_party/7zip/win/arm64/`）；
  - 可选的资源管理器右键菜单安装/卸载 PowerShell 脚本。
- GitHub Actions：
  - `build.yml` 可复用构建矩阵：Linux amd64/arm64 原生 deb、Windows x64
    原生构建与测试、Windows ARM64 交叉构建；
  - `ci.yml` 在 push / PR 上自动构建测试；
  - `release.yml` 在推送 `v*` 标签时构建四种产物、生成 `SHA256SUMS` 并创建
    GitHub Release。
- Debian 打包：`debian/control`、`debian/rules`、changelog、copyright；
  可用 `mk-build-deps` 安装构建依赖。
- 命令行：`QArchiver <archive>`、`--oneclick <archive>`、`--preferences`、
  `--benchmark`、`--lang`。

### Changed / 变更

- 源码按层重构为静态库：`common`（格式表/错误/数据结构）、`utils`、`backends`、
  `core`、`ui`，依赖只能自上而下，消除了 `core → backends → utils → core`
  的循环依赖；`main.cpp` 精简为启动流程，命令行解析与翻译安装移入 `app/`。
- 拆分为多个编译单元以降低单文件体积：`ArchiveManager` 拆出
  `ArchiveBackendFactory`、`ExtractionPreflight`、`CompressOutput`、
  `ArchiveManagerDispatch`；`MainWindow` 按职责拆为动作/布局/导航/解压/拖出/
  公共工具六个实现文件。
- 统一图标与主题资源：全部移到仓库根目录的 `resources/`，按用途分为
  `icons/classic`、`icons/modern`、`icons/file-types`、`themes/aero`、
  `themes/modern`；两套主题的 `light/` 与 `dark/` 使用相同文件名，深浅共用
  的字形放在主题根目录；资源路径只在 `Icons::` 与 `ThemeManager` 中拼接，
  资源路径恒等于文件在 `resources/` 下的路径。
- 格式后缀与类型判断统一到 `FormatRegistry`（`MimeTypeHelper`、`FileTypeHelper`、
  `CompressDialog`、一键解压均改为查表），`backends/zip` 合并进 `backends/sevenzip`，
  `backends/archive` 更名为 `backends/libarchive`。
- 全部静态库 `POSITION_INDEPENDENT_CODE ON`，libarchive 探测移入
  `cmake/QArchiverLibArchive.cmake`；测试统一通过 `qarchiver_add_test()` 注册。
- 图标归一：经典图标工具栏 48px、列表图标 64px；新增 `tools/normalize_classic_icons.py`。
- 偏好设置的“界面”页按当前布局隐藏无效选项：现代布局隐藏“工具栏样式”和
  “显示最近文件侧边栏”，经典布局隐藏“归档内容视图”（经典布局没有视图切换入口，
  固定使用树视图）；“最近文件上限”与背景图片在两种布局下都保留。
  切换布局下拉框时立即更新，无需保存后重开对话框。
- 默认界面由 Aero/Windows 7 风格改为现代扁平风格；经典布局与 Aero 主题仍保留。
- 归档内容默认视图由树形改为网格；已有显式列表/树形设置会保留。
- 文件列表新增“类型”列，现代布局隐藏旧“加密”列；现代布局使用彩色文件类型图标。
- 只读 libarchive 后端上的 zip/7z/未压缩 tar 添加/删除会自动切换到命令行后端重放，
  不再直接禁用“添加/删除”。
- 压缩包内无压缩后大小时显示 `-`，状态栏不再显示误导性的 100% 压缩比。

### Fixed / 修复

- `-DQARCHIVER_USE_LIBARCHIVE=OFF` 时测试编译失败：`ArchiveBackendFactory.h`
  的 include 被放进了 `#ifdef QARCHIVER_HAS_LIBARCHIVE` 内部。
- 资源自检脚本漏检两类问题：`Icons::` 调用字面量与 QSS 中的 `url(:/...)`；
  另新增两套主题 `light/`/`dark/` 文件名一致性与资源路径存在性校验。
- `MimeTypeHelper::resolveStreamFormat()` 改用 `FormatRegistry` 判断复合 tar 后缀，
  不再维护第四份后缀表。
- 修正 tar/7z 在用户跳过危险符号链接时仍可能解出该链接的问题：
  归档根目录 `.` 条目会让 7-Zip 展开整棵树，现已从成员列表中排除。
- 一键解压对 `.7z.001` / `.part1.rar` 等分卷使用正确的目标文件夹名。
- 只读后端写回退期间保留当前列表，取消或失败后导航/摘要不会停留在“正在打开”。
- 修正后台取色/透明度测试在不同渲染后端上的舍入误差。
- 清理 mk-build-deps 遗留文件与 Qt 5 时代文档描述。

[0.1.2]: https://github.com/shenmo7192/QArchiver/releases/tag/v0.1.2
