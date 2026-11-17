<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="zh_CN">
<context>
    <name>ArchiveManager</name>
    <message>
        <source>Another operation is already in progress</source>
        <translation>另一个操作正在进行中</translation>
    </message>
    <message>
        <source>Failed to dispatch the archive operation</source>
        <translation>无法派发压缩包操作</translation>
    </message>
    <message>
        <source>File does not exist: %1</source>
        <translation>文件不存在：%1</translation>
    </message>
    <message>
        <source>File is not readable: %1</source>
        <translation>文件不可读：%1</translation>
    </message>
    <message>
        <source>Unrecognized archive format or file is corrupted: %1</source>
        <translation>无法识别的压缩格式或文件已损坏：%1</translation>
    </message>
    <message>
        <source>No backend available for format: %1</source>
        <translation>没有可用于该格式的后端：%1</translation>
    </message>
    <message>
        <source>Failed to open archive: %1</source>
        <translation>无法打开压缩包：%1</translation>
    </message>
    <message>
        <source>No destination directory selected</source>
        <translation>未选择目标目录</translation>
    </message>
    <message>
        <source>Blocked unsafe path: %1</source>
        <translation>已阻止不安全的路径：%1</translation>
    </message>
    <message>
        <source>Insufficient disk space: %1 required, %2 available</source>
        <translation>磁盘空间不足：需要 %1，可用 %2</translation>
    </message>
    <message>
        <source>Cannot create destination directory: %1</source>
        <translation>无法创建目标目录：%1</translation>
    </message>
    <message>
        <source>No archive name specified</source>
        <translation>未指定压缩包名称</translation>
    </message>
    <message>
        <source>No source files selected</source>
        <translation>未选择源文件</translation>
    </message>
    <message>
        <source>Cannot create target directory: %1</source>
        <translation>无法创建目标目录：%1</translation>
    </message>
    <message>
        <source>Cannot create a temporary directory next to the target</source>
        <translation>无法在目标位置旁创建临时目录</translation>
    </message>
    <message>
        <source>Failed to dispatch the compression operation</source>
        <translation>无法派发压缩操作</translation>
    </message>
    <message>
        <source>Cancelled</source>
        <translation>已取消</translation>
    </message>
    <message>
        <source>Failed to finalize the compressed archive</source>
        <translation>无法完成压缩包写入</translation>
    </message>
    <message>
        <source>libarchive cannot handle this archive, switching to the 7z command line.</source>
        <translation>libarchive 无法处理此压缩包，改用 7z 命令行。</translation>
    </message>
    <message>
        <source>No archive open</source>
        <translation>未打开压缩包</translation>
    </message>
    <message>
        <source>This archive does not support adding files</source>
        <translation>该压缩包不支持添加文件</translation>
    </message>
    <message>
        <source>This archive does not support deleting files</source>
        <translation>该压缩包不支持删除文件</translation>
    </message>
    <message>
        <source>Too many failed password attempts.</source>
        <translation>密码错误次数过多。</translation>
    </message>
</context>
<context>
    <name>CompressDialog</name>
    <message>
        <source>Create Archive</source>
        <translation>新建压缩包</translation>
    </message>
    <message>
        <source>Archive name:</source>
        <translation>压缩包名称：</translation>
    </message>
    <message>
        <source>Browse...</source>
        <translation>浏览...</translation>
    </message>
    <message>
        <source>7z (.7z)</source>
        <translation>7z (.7z)</translation>
    </message>
    <message>
        <source>Zip (.zip)</source>
        <translation>Zip (.zip)</translation>
    </message>
    <message>
        <source>Tar (.tar)</source>
        <translation>Tar (.tar)</translation>
    </message>
    <message>
        <source>Gzip tar (.tar.gz)</source>
        <translation>Gzip tar (.tar.gz)</translation>
    </message>
    <message>
        <source>Xz tar (.tar.xz)</source>
        <translation>Xz tar (.tar.xz)</translation>
    </message>
    <message>
        <source>Bzip2 tar (.tar.bz2)</source>
        <translation>Bzip2 tar (.tar.bz2)</translation>
    </message>
    <message>
        <source>(no encryption)</source>
        <translation>（不加密）</translation>
    </message>
    <message>
        <source>Encrypt file names (7z only)</source>
        <translation>加密文件名（仅 7z）</translation>
    </message>
    <message>
        <source>MB</source>
        <translation>MB</translation>
    </message>
    <message>
        <source>KB</source>
        <translation>KB</translation>
    </message>
    <message>
        <source>Follow symlinks</source>
        <translation>跟随符号链接</translation>
    </message>
    <message>
        <source>Solid compression (7z only)</source>
        <translation>固实压缩（仅 7z）</translation>
    </message>
    <message>
        <source>Store full/absolute paths</source>
        <translation>保存完整/绝对路径</translation>
    </message>
    <message>
        <source>Patterns separated by &apos;;&apos; (e.g. *.tmp;build/*)</source>
        <translation>多个模式用“;”分隔（例如 *.tmp;build/*）</translation>
    </message>
    <message>
        <source>&amp;Format:</source>
        <translation>格式(&amp;F)：</translation>
    </message>
    <message>
        <source>Compression &amp;level:</source>
        <translation>压缩级别(&amp;L)：</translation>
    </message>
    <message>
        <source>&amp;Password:</source>
        <translation>密码(&amp;P)：</translation>
    </message>
    <message>
        <source>&amp;Volume size (0=no split):</source>
        <translation>分卷大小(&amp;V)（0=不分割）：</translation>
    </message>
    <message>
        <source>&amp;Exclude patterns:</source>
        <translation>排除模式(&amp;E)：</translation>
    </message>
    <message>
        <source>Select Archive Name</source>
        <translation>选择压缩包名称</translation>
    </message>
    <message>
        <source>Archives (*%1)</source>
        <translation>压缩包 (*%1)</translation>
    </message>
    <message>
        <source>Please specify an archive name.</source>
        <translation>请指定压缩包名称。</translation>
    </message>
    <message>
        <source>No source files were selected.</source>
        <translation>未选择源文件。</translation>
    </message>
    <message>
        <source>Overwrite Archive</source>
        <translation>覆盖压缩包</translation>
    </message>
    <message>
        <source>The archive already exists:
%1

Overwrite it?</source>
        <translation>压缩包已存在：
%1

是否覆盖？</translation>
    </message>
</context>
<context>
    <name>DragOutManager</name>
    <message>
        <source>No archive is open.</source>
        <translation>未打开任何压缩包。</translation>
    </message>
    <message>
        <source>No entries selected.</source>
        <translation>未选中任何条目。</translation>
    </message>
    <message>
        <source>Cannot create a temporary directory for dragging.</source>
        <translation>无法为拖拽创建临时目录。</translation>
    </message>
    <message>
        <source>Blocked unsafe path: %1</source>
        <translation>已阻止不安全的路径：%1</translation>
    </message>
    <message>
        <source>Insufficient disk space for drag preparation.</source>
        <translation>磁盘空间不足，无法准备拖拽文件。</translation>
    </message>
    <message>
        <source>Extracting selected files...</source>
        <translation>正在解压选中的文件…</translation>
    </message>
    <message>
        <source>Unable to prepare the selected files for dragging.</source>
        <translation>无法为拖拽准备选中的文件。</translation>
    </message>
    <message>
        <source>Finalizing drag data...</source>
        <translation>正在整理拖拽数据…</translation>
    </message>
    <message>
        <source>The selected entries were not extracted.</source>
        <translation>选中的条目没有解压出来。</translation>
    </message>
</context>
<context>
    <name>ExtractDialog</name>
    <message>
        <source>Extract Archive</source>
        <translation>解压压缩包</translation>
    </message>
    <message>
        <source>Destination:</source>
        <translation>目标位置：</translation>
    </message>
    <message>
        <source>Browse...</source>
        <translation>浏览...</translation>
    </message>
    <message>
        <source>Preserve directory structure</source>
        <translation>保留目录结构</translation>
    </message>
    <message>
        <source>Open destination folder after extraction</source>
        <translation>解压后打开目标文件夹</translation>
    </message>
    <message>
        <source>Test archive integrity before extraction</source>
        <translation>解压前测试压缩包完整性</translation>
    </message>
    <message>
        <source>Ask</source>
        <translation>询问</translation>
    </message>
    <message>
        <source>Overwrite</source>
        <translation>覆盖</translation>
    </message>
    <message>
        <source>Skip existing</source>
        <translation>跳过已有文件</translation>
    </message>
    <message>
        <source>Auto-rename</source>
        <translation>自动重命名</translation>
    </message>
    <message>
        <source>&amp;Overwrite policy:</source>
        <translation>覆盖策略(&amp;O)：</translation>
    </message>
    <message>
        <source>Select Destination</source>
        <translation>选择目标位置</translation>
    </message>
    <message>
        <source>Extract Selected Entries (%1)</source>
        <translation>解压选中条目（%1）</translation>
    </message>
</context>
<context>
    <name>FileListModel</name>
    <message>
        <source>Parent directory</source>
        <translation>上一级目录</translation>
    </message>
    <message>
        <source>Yes</source>
        <translation>是</translation>
    </message>
    <message>
        <source>Name</source>
        <translation>名称</translation>
    </message>
    <message>
        <source>Size</source>
        <translation>大小</translation>
    </message>
    <message>
        <source>Packed</source>
        <translation>压缩后大小</translation>
    </message>
    <message>
        <source>Modified</source>
        <translation>修改时间</translation>
    </message>
    <message>
        <source>Encrypted</source>
        <translation>已加密</translation>
    </message>
</context>
<context>
    <name>LibArchiveBackend</name>
    <message>
        <source>File not found: %1</source>
        <translation>文件未找到：%1</translation>
    </message>
    <message>
        <source>File is not readable: %1</source>
        <translation>文件不可读：%1</translation>
    </message>
    <message>
        <source>Wrong password</source>
        <translation>密码错误</translation>
    </message>
    <message>
        <source>%1: %2</source>
        <translation>%1：%2</translation>
    </message>
    <message>
        <source>Archive not opened</source>
        <translation>压缩包未打开</translation>
    </message>
    <message>
        <source>Cannot open archive</source>
        <translation>无法打开压缩包</translation>
    </message>
    <message>
        <source>Cannot read archive</source>
        <translation>无法读取压缩包</translation>
    </message>
    <message>
        <source>No destination directory selected</source>
        <translation>未选择目标目录</translation>
    </message>
    <message>
        <source>Cannot create a temporary directory in %1</source>
        <translation>无法在 %1 中创建临时目录</translation>
    </message>
    <message>
        <source>Blocked unsafe path: %1</source>
        <translation>已阻止不安全的路径：%1</translation>
    </message>
    <message>
        <source>Cannot create directory: %1</source>
        <translation>无法创建目录：%1</translation>
    </message>
    <message>
        <source>Skipped unsafe symlink: %1</source>
        <translation>已跳过不安全的符号链接：%1</translation>
    </message>
    <message>
        <source>Cannot write file: %1</source>
        <translation>无法写入文件：%1</translation>
    </message>
    <message>
        <source>Extraction failed</source>
        <translation>解压失败</translation>
    </message>
    <message>
        <source>Operation cancelled</source>
        <translation>操作已取消</translation>
    </message>
    <message>
        <source>Failed to move %1 extracted entries to %2</source>
        <translation>有 %1 个条目无法移动到 %2</translation>
    </message>
    <message>
        <source>Extraction completed</source>
        <translation>解压完成</translation>
    </message>
    <message>
        <source>Creating archives is not supported by the libarchive backend</source>
        <translation>libarchive 后端不支持创建压缩包</translation>
    </message>
    <message>
        <source>Adding files is not supported by the libarchive backend</source>
        <translation>libarchive 后端不支持添加文件</translation>
    </message>
    <message>
        <source>Deleting files is not supported by the libarchive backend</source>
        <translation>libarchive 后端不支持删除文件</translation>
    </message>
    <message>
        <source>Integrity test failed</source>
        <translation>完整性校验失败</translation>
    </message>
    <message>
        <source>Archive integrity verified</source>
        <translation>压缩包完整性校验通过</translation>
    </message>
</context>
<context>
    <name>MainWindow</name>
    <message>
        <source>QArchiver</source>
        <translation>QArchiver</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>打开</translation>
    </message>
    <message>
        <source>New Archive</source>
        <translation>新建压缩包</translation>
    </message>
    <message>
        <source>Extract</source>
        <translation>解压</translation>
    </message>
    <message>
        <source>Add Files...</source>
        <translation>添加文件...</translation>
    </message>
    <message>
        <source>Delete Selected</source>
        <translation>删除选中项</translation>
    </message>
    <message>
        <source>Close</source>
        <translation>关闭</translation>
    </message>
    <message>
        <source>Main</source>
        <translation>主工具栏</translation>
    </message>
    <message>
        <source>Preferences</source>
        <translation>偏好设置</translation>
    </message>
    <message>
        <source>About</source>
        <translation>关于</translation>
    </message>
    <message>
        <source>Quit</source>
        <translation>退出</translation>
    </message>
    <message>
        <source>No archive open.
Use the Open button or drag a file here.</source>
        <translation>未打开压缩包。
请使用“打开”按钮或将文件拖到此处。</translation>
    </message>
    <message>
        <source>Choose Archive...</source>
        <translation>选择压缩包...</translation>
    </message>
    <message>
        <source>Filter by name or path (wildcards allowed)...</source>
        <translation>按名称或路径筛选（支持通配符）...</translation>
    </message>
    <message>
        <source>Ready</source>
        <translation>就绪</translation>
    </message>
    <message>
        <source>Recent Files</source>
        <translation>最近文件</translation>
    </message>
    <message>
        <source>Overwrite Files</source>
        <translation>覆盖文件</translation>
    </message>
    <message>
        <source>The destination directory is not empty. Overwrite existing files?</source>
        <translation>目标目录不为空。是否覆盖已有文件？</translation>
    </message>
    <message>
        <source>Extracting...</source>
        <translation>正在解压...</translation>
    </message>
    <message>
        <source>Add Files</source>
        <translation>添加文件</translation>
    </message>
    <message>
        <source>This archive does not support adding files.</source>
        <translation>此压缩包不支持添加文件。</translation>
    </message>
    <message>
        <source>Adding Files...</source>
        <translation>正在添加文件...</translation>
    </message>
    <message>
        <source>Open Archive</source>
        <translation>打开压缩包</translation>
    </message>
    <message>
        <source>Archives (*.tar *.tar.gz *.tgz *.tar.xz *.txz *.tar.bz2 *.tbz2 *.zip *.7z *.rar *.gz *.bz2 *.xz *.001);;All Files (*)</source>
        <translation>压缩包 (*.tar *.tar.gz *.tgz *.tar.xz *.txz *.tar.bz2 *.tbz2 *.zip *.7z *.rar *.gz *.bz2 *.xz *.001);;所有文件 (*)</translation>
    </message>
    <message>
        <source>Select Files to Compress</source>
        <translation>选择要压缩的文件</translation>
    </message>
    <message>
        <source>Creating Archive...</source>
        <translation>正在创建压缩包...</translation>
    </message>
    <message>
        <source>Select Files to Add</source>
        <translation>选择要添加的文件</translation>
    </message>
    <message>
        <source>Delete Files</source>
        <translation>删除文件</translation>
    </message>
    <message>
        <source>This archive does not support deleting files.</source>
        <translation>此压缩包不支持删除文件。</translation>
    </message>
    <message>
        <source>Delete Selected Entries</source>
        <translation>删除选中条目</translation>
    </message>
    <message>
        <source>Delete %1 selected entr%2 from the archive?

This cannot be undone.</source>
        <translation>确定要从压缩包中删除选中的 %1 个条目吗？

此操作无法撤销。</translation>
    </message>
    <message>
        <source>Deleting Entries...</source>
        <translation>正在删除条目...</translation>
    </message>
    <message>
        <source>Archive closed</source>
        <translation>压缩包已关闭</translation>
    </message>
    <message>
        <source>About QArchiver</source>
        <translation>关于 QArchiver</translation>
    </message>
    <message>
        <source>Extract Selected...</source>
        <translation>解压选中项...</translation>
    </message>
    <message>
        <source>QArchiver - %1</source>
        <translation>QArchiver - %1</translation>
    </message>
    <message>
        <source>Opened: %1</source>
        <translation>已打开：%1</translation>
    </message>
    <message>
        <source>Reading archive %1</source>
        <translation>正在读取压缩包 %1</translation>
    </message>
    <message>
        <source>%1 entries</source>
        <translation>%1 个条目</translation>
    </message>
    <message>
        <source>Processing...</source>
        <translation>正在处理...</translation>
    </message>
    <message>
        <source>Error</source>
        <translation>错误</translation>
    </message>
    <message>
        <source>Unsupported Format</source>
        <translation>不支持的格式</translation>
    </message>
    <message>
        <source>Wrong Password</source>
        <translation>密码错误</translation>
    </message>
    <message>
        <source>Corrupted Archive</source>
        <translation>压缩包已损坏</translation>
    </message>
    <message>
        <source>Required Tool Not Found</source>
        <translation>未找到所需工具</translation>
    </message>
    <message>
        <source>Operation Not Supported</source>
        <translation>不支持该操作</translation>
    </message>
    <message>
        <source>Password Required</source>
        <translation>需要密码</translation>
    </message>
    <message>
        <source>Insufficient Disk Space</source>
        <translation>磁盘空间不足</translation>
    </message>
    <message>
        <source>Security: Path Traversal Blocked</source>
        <translation>安全：已阻止路径穿越</translation>
    </message>
    <message>
        <source>Volume Missing</source>
        <translation>分卷缺失</translation>
    </message>
    <message>
        <source>File Not Found</source>
        <translation>文件未找到</translation>
    </message>
    <message>
        <source>Permission Denied</source>
        <translation>权限不足</translation>
    </message>
    <message>
        <source>Path Too Long</source>
        <translation>路径过长</translation>
    </message>
    <message>
        <source>Encoding Error</source>
        <translation>编码错误</translation>
    </message>
    <message>
        <source>Cancelled</source>
        <translation>已取消</translation>
    </message>
    <message>
        <source>&lt;h3&gt;QArchiver (小Q解压)&lt;/h3&gt;&lt;p&gt;A lightweight Qt-based archive manager for Linux.&lt;/p&gt;&lt;p&gt;Supports: tar, tar.gz, tar.xz, tar.bz2, zip, 7z, rar, gz, bz2, xz&lt;/p&gt;&lt;p&gt;Version: %1&lt;/p&gt;</source>
        <translation>&lt;h3&gt;QArchiver（小Q解压）&lt;/h3&gt;&lt;p&gt;轻量级 Qt 压缩包管理工具，适用于 Linux。&lt;/p&gt;&lt;p&gt;支持：tar、tar.gz、tar.xz、tar.bz2、zip、7z、rar、gz、bz2、xz&lt;/p&gt;&lt;p&gt;版本：%1&lt;/p&gt;</translation>
    </message>
    <message>
        <source>Detecting format: %1</source>
        <translation>正在检测格式：%1</translation>
    </message>
    <message>
        <source>Detecting archive format...</source>
        <translation>正在检测压缩包格式...</translation>
    </message>
    <message>
        <source>Unable to open archive.
%1</source>
        <translation>无法打开压缩包。
%1</translation>
    </message>
    <message>
        <source>Operation failed</source>
        <translation>操作失败</translation>
    </message>
    <message>
        <source>Drag Out</source>
        <translation>拖拽</translation>
    </message>
    <message>
        <source>Cannot drop entries onto QArchiver itself.
Please drop them onto a file manager, the desktop, or another application.</source>
        <translation>不能把条目拖放到 QArchiver 自身。
请拖到文件管理器、桌面或其他程序中。</translation>
    </message>
    <message>
        <source>The target application does not accept file drops.</source>
        <translation>目标程序不接受文件拖放。</translation>
    </message>
    <message>
        <source>Too many failed password attempts.</source>
        <translation>密码错误次数过多。</translation>
    </message>
    <message>
        <source>Preparing files for drag...</source>
        <translation>正在准备拖拽文件…</translation>
    </message>
    <message>
        <source>Preparing files for drag... %1%</source>
        <translation>正在准备拖拽文件… %1%</translation>
    </message>
    <message>
        <source>Files are ready. Drag the selection again to copy them out.</source>
        <translation>文件已准备就绪，请再次拖动选中的项目即可复制出去。</translation>
    </message>
</context>
<context>
    <name>PasswordDialog</name>
    <message>
        <source>Enter Password</source>
        <translation>输入密码</translation>
    </message>
    <message>
        <source>This archive is encrypted. Please enter the password:</source>
        <translation>该压缩包已加密。请输入密码：</translation>
    </message>
    <message>
        <source>Password</source>
        <translation>密码</translation>
    </message>
    <message>
        <source>Remember password for this session</source>
        <translation>本次会话记住密码</translation>
    </message>
    <message>
        <source>&amp;Password:</source>
        <translation>密码(&amp;P)：</translation>
    </message>
</context>
<context>
    <name>PreferencesDialog</name>
    <message>
        <source>Preferences</source>
        <translation>偏好设置</translation>
    </message>
    <message>
        <source>Zip</source>
        <translation>Zip</translation>
    </message>
    <message>
        <source>7z</source>
        <translation>7z</translation>
    </message>
    <message>
        <source>Tar</source>
        <translation>Tar</translation>
    </message>
    <message>
        <source>Tar.gz</source>
        <translation>Tar.gz</translation>
    </message>
    <message>
        <source>Ask</source>
        <translation>询问</translation>
    </message>
    <message>
        <source>Always overwrite</source>
        <translation>总是覆盖</translation>
    </message>
    <message>
        <source>Never overwrite</source>
        <translation>从不覆盖</translation>
    </message>
    <message>
        <source>Preserve directory structure</source>
        <translation>保留目录结构</translation>
    </message>
    <message>
        <source>Open destination after extraction</source>
        <translation>解压后打开目标文件夹</translation>
    </message>
    <message>
        <source> MB</source>
        <translation> MB</translation>
    </message>
    <message>
        <source>Follow symlinks when compressing</source>
        <translation>压缩时跟随符号链接</translation>
    </message>
    <message>
        <source>Remember passwords for the session</source>
        <translation>在本次会话中记住密码</translation>
    </message>
    <message>
        <source>Passwords are kept in memory only and are never written to disk.</source>
        <translation>密码只保存在内存中，绝不会写入磁盘。</translation>
    </message>
    <message>
        <source>System</source>
        <translation>跟随系统</translation>
    </message>
    <message>
        <source>Light</source>
        <translation>浅色</translation>
    </message>
    <message>
        <source>Dark</source>
        <translation>深色</translation>
    </message>
    <message>
        <source>Tree view</source>
        <translation>树状视图</translation>
    </message>
    <message>
        <source>List view</source>
        <translation>列表视图</translation>
    </message>
    <message>
        <source>Icons above text</source>
        <translation>图标在上，文本在下</translation>
    </message>
    <message>
        <source>Icons beside text</source>
        <translation>图标在左，文本在右</translation>
    </message>
    <message>
        <source>Text only</source>
        <translation>纯文本</translation>
    </message>
    <message>
        <source>Icons only (tooltips)</source>
        <translation>纯图标（鼠标悬浮提示文本）</translation>
    </message>
    <message>
        <source>Read archives with libarchive (no external process)</source>
        <translation>使用 libarchive 读取压缩包（不启动外部程序）</translation>
    </message>
    <message>
        <source>zip, tar, 7z and gz/bz2/xz archives are read inside the application. Archives that libarchive cannot open (encrypted 7z, split volumes, rar) automatically switch to the command line tools. The change applies to the next archive you open.</source>
        <translation>zip、tar、7z 以及 gz/bz2/xz 压缩包在程序内部读取。libarchive 无法打开的压缩包（加密 7z、分卷、rar）会自动改用命令行工具。此设置在下一次打开压缩包时生效。</translation>
    </message>
    <message>
        <source>Disabled by the QARCHIVER_NO_LIBARCHIVE environment variable.</source>
        <translation>已在环境变量 QARCHIVER_NO_LIBARCHIVE 中禁用。</translation>
    </message>
    <message>
        <source>This build was compiled without libarchive.</source>
        <translation>本次构建未包含 libarchive。</translation>
    </message>
    <message>
        <source>Default compression &amp;format:</source>
        <translation>默认压缩格式(&amp;F)：</translation>
    </message>
    <message>
        <source>Default compression &amp;level:</source>
        <translation>默认压缩级别(&amp;L)：</translation>
    </message>
    <message>
        <source>&amp;Overwrite policy:</source>
        <translation>覆盖策略(&amp;O)：</translation>
    </message>
    <message>
        <source>Max preview &amp;size:</source>
        <translation>预览大小上限(&amp;S)：</translation>
    </message>
    <message>
        <source>&amp;Theme:</source>
        <translation>主题(&amp;T)：</translation>
    </message>
    <message>
        <source>Archive content &amp;view:</source>
        <translation>压缩包内容视图(&amp;V)：</translation>
    </message>
    <message>
        <source>Toolbar button style:</source>
        <translation>工具栏按钮样式：</translation>
    </message>
    <message>
        <source>Show recent files sidebar</source>
        <translation>显示最近文件侧边栏</translation>
    </message>
    <message>
        <source>Applies to the next archive you open; the currently open archive keeps the backend it started with.</source>
        <translation>此设置在下一次打开压缩包时生效；当前已打开的压缩包继续使用原来的读取方式，不会被打断。</translation>
    </message>
</context>
<context>
    <name>PreviewManager</name>
    <message>
        <source>Preview</source>
        <translation>预览</translation>
    </message>
    <message>
        <source>This file is too large to preview (%1).</source>
        <translation>此文件过大，无法预览（%1）。</translation>
    </message>
</context>
<context>
    <name>ProgressDialog</name>
    <message>
        <source>Working...</source>
        <translation>正在处理...</translation>
    </message>
    <message>
        <source>Processing...</source>
        <translation>正在处理...</translation>
    </message>
    <message>
        <source>Cancel</source>
        <translation>取消</translation>
    </message>
    <message>
        <source>Cancelling...</source>
        <translation>正在取消...</translation>
    </message>
    <message>
        <source>Cancelling operation...</source>
        <translation>正在取消操作...</translation>
    </message>
    <message>
        <source>%1 / %2 files</source>
        <translation>%1 / %2 个文件</translation>
    </message>
    <message>
        <source> - ETA: %1:%2</source>
        <translation> - 预计剩余时间：%1:%2</translation>
    </message>
    <message>
        <source> - ETA: unknown</source>
        <translation> - 预计剩余时间：未知</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <source>No backend available for preview</source>
        <translation>没有可用于预览的后端</translation>
    </message>
    <message>
        <source>Preview</source>
        <translation>预览</translation>
    </message>
    <message>
        <source>Unable to extract this file for preview.</source>
        <translation>无法解压此文件进行预览。</translation>
    </message>
    <message>
        <source>No backend available.</source>
        <translation>没有可用的后端。</translation>
    </message>
    <message>
        <source>Password is required.</source>
        <translation>需要密码。</translation>
    </message>
    <message>
        <source>Retrying with the command line backend...</source>
        <translation>正在改用命令行后端重试…</translation>
    </message>
</context>
<context>
    <name>RarBackend</name>
    <message>
        <source>File not found: %1</source>
        <translation>文件未找到：%1</translation>
    </message>
    <message>
        <source>File is not readable: %1</source>
        <translation>文件不可读：%1</translation>
    </message>
    <message>
        <source>Archive not opened</source>
        <translation>压缩包未打开</translation>
    </message>
    <message>
        <source>unrar tool not found</source>
        <translation>未找到 unrar 工具</translation>
    </message>
    <message>
        <source>Operation cancelled</source>
        <translation>操作已取消</translation>
    </message>
    <message>
        <source>Failed to start unrar</source>
        <translation>无法启动 unrar</translation>
    </message>
    <message>
        <source>Wrong password</source>
        <translation>密码错误</translation>
    </message>
    <message>
        <source>List failed: %1</source>
        <translation>列出内容失败：%1</translation>
    </message>
    <message>
        <source>Blocked unsafe path: %1</source>
        <translation>已阻止不安全的路径：%1</translation>
    </message>
    <message>
        <source>Extraction failed (wrong password or corrupted)</source>
        <translation>解压失败（密码错误或文件损坏）</translation>
    </message>
    <message>
        <source>Extraction failed (exit code %1)</source>
        <translation>解压失败（退出码 %1）</translation>
    </message>
    <message>
        <source>Extraction completed, but %1 entr%2 could not be moved</source>
        <translation>解压完成，但有 %1 个条目无法移动</translation>
    </message>
    <message>
        <source>Extraction completed</source>
        <translation>解压完成</translation>
    </message>
    <message>
        <source>RAR compression is not supported (RARLAB license)</source>
        <translation>不支持 RAR 压缩（受 RARLAB 许可限制）</translation>
    </message>
    <message>
        <source>RAR does not support adding files (read-only)</source>
        <translation>RAR 不支持添加文件（只读）</translation>
    </message>
    <message>
        <source>RAR does not support deleting files (read-only)</source>
        <translation>RAR 不支持删除文件（只读）</translation>
    </message>
    <message>
        <source>Archive test failed</source>
        <translation>压缩包测试失败</translation>
    </message>
    <message>
        <source>Archive integrity verified</source>
        <translation>压缩包完整性校验通过</translation>
    </message>
</context>
<context>
    <name>SevenZipBackend</name>
    <message>
        <source>File not found: %1</source>
        <translation>文件未找到：%1</translation>
    </message>
    <message>
        <source>File is not readable: %1</source>
        <translation>文件不可读：%1</translation>
    </message>
    <message>
        <source>Archive not opened</source>
        <translation>压缩包未打开</translation>
    </message>
    <message>
        <source>7z tool not found</source>
        <translation>未找到 7z 工具</translation>
    </message>
    <message>
        <source>Operation cancelled</source>
        <translation>操作已取消</translation>
    </message>
    <message>
        <source>Failed to start 7z</source>
        <translation>无法启动 7z</translation>
    </message>
    <message>
        <source>Wrong password</source>
        <translation>密码错误</translation>
    </message>
    <message>
        <source>List failed: %1</source>
        <translation>列出内容失败：%1</translation>
    </message>
    <message>
        <source>Blocked unsafe path: %1</source>
        <translation>已阻止不安全的路径：%1</translation>
    </message>
    <message>
        <source>Extraction failed: %1</source>
        <translation>解压失败：%1</translation>
    </message>
    <message>
        <source>Extraction completed, but %1 entr%2 could not be moved</source>
        <translation>解压完成，但有 %1 个条目无法移动</translation>
    </message>
    <message>
        <source>Extraction completed</source>
        <translation>解压完成</translation>
    </message>
    <message>
        <source>Compression failed: %1</source>
        <translation>压缩失败：%1</translation>
    </message>
    <message>
        <source>Archive created: %1</source>
        <translation>压缩包已创建：%1</translation>
    </message>
    <message>
        <source>Cannot add files to a split archive</source>
        <translation>无法向分卷压缩包添加文件</translation>
    </message>
    <message>
        <source>Add files failed: %1</source>
        <translation>添加文件失败：%1</translation>
    </message>
    <message>
        <source>Files added to archive</source>
        <translation>文件已添加到压缩包</translation>
    </message>
    <message>
        <source>Cannot delete files from a split archive</source>
        <translation>无法从分卷压缩包删除文件</translation>
    </message>
    <message>
        <source>Delete files failed: %1</source>
        <translation>删除文件失败：%1</translation>
    </message>
    <message>
        <source>Files deleted from archive</source>
        <translation>文件已从压缩包删除</translation>
    </message>
    <message>
        <source>Archive test failed</source>
        <translation>压缩包测试失败</translation>
    </message>
    <message>
        <source>Archive integrity verified</source>
        <translation>压缩包完整性校验通过</translation>
    </message>
</context>
<context>
    <name>SidebarWidget</name>
    <message>
        <source>Recently opened archives. Double-click to open.</source>
        <translation>最近打开的压缩包。双击打开。</translation>
    </message>
    <message>
        <source>Open</source>
        <translation>打开</translation>
    </message>
    <message>
        <source>Remove from list</source>
        <translation>从列表移除</translation>
    </message>
    <message>
        <source>Clear missing entries</source>
        <translation>清除失效条目</translation>
    </message>
</context>
<context>
    <name>StreamBackend</name>
    <message>
        <source>File not found: %1</source>
        <translation>文件未找到：%1</translation>
    </message>
    <message>
        <source>File is not readable: %1</source>
        <translation>文件不可读：%1</translation>
    </message>
    <message>
        <source>Archive not opened</source>
        <translation>压缩包未打开</translation>
    </message>
    <message>
        <source>7z tool not found</source>
        <translation>未找到 7z 工具</translation>
    </message>
    <message>
        <source>Operation cancelled</source>
        <translation>操作已取消</translation>
    </message>
    <message>
        <source>Failed to start 7z</source>
        <translation>无法启动 7z</translation>
    </message>
    <message>
        <source>Decompression failed (corrupted stream)</source>
        <translation>解压失败（数据流损坏）</translation>
    </message>
    <message>
        <source>The stream did not contain any file</source>
        <translation>该流中不包含任何文件</translation>
    </message>
    <message>
        <source>Failed to move extracted file to %1</source>
        <translation>无法将解压文件移动到 %1</translation>
    </message>
    <message>
        <source>Decompression completed</source>
        <translation>解压完成</translation>
    </message>
    <message>
        <source>Creating pure compression streams is not supported</source>
        <translation>不支持创建纯压缩流</translation>
    </message>
    <message>
        <source>Cannot add files to a pure compression stream</source>
        <translation>无法向纯压缩流添加文件</translation>
    </message>
    <message>
        <source>Cannot delete files from a pure compression stream</source>
        <translation>无法从纯压缩流删除文件</translation>
    </message>
    <message>
        <source>Stream test failed</source>
        <translation>数据流测试失败</translation>
    </message>
    <message>
        <source>Stream integrity verified</source>
        <translation>数据流完整性校验通过</translation>
    </message>
</context>
<context>
    <name>TarBackend</name>
    <message>
        <source>File does not exist: %1</source>
        <translation>文件不存在：%1</translation>
    </message>
    <message>
        <source>File is not readable: %1</source>
        <translation>文件不可读：%1</translation>
    </message>
    <message>
        <source>Archive not opened</source>
        <translation>压缩包未打开</translation>
    </message>
    <message>
        <source>tar tool not found</source>
        <translation>未找到 tar 工具</translation>
    </message>
    <message>
        <source>Operation cancelled</source>
        <translation>操作已取消</translation>
    </message>
    <message>
        <source>Failed to start tar</source>
        <translation>无法启动 tar</translation>
    </message>
    <message>
        <source>This does not look like a tar archive: %1</source>
        <translation>这看起来不像 tar 压缩包：%1</translation>
    </message>
    <message>
        <source>Failed to list archive</source>
        <translation>无法列出压缩包内容</translation>
    </message>
    <message>
        <source>Blocked unsafe path: %1</source>
        <translation>已阻止不安全的路径：%1</translation>
    </message>
    <message>
        <source>Extraction failed: %1</source>
        <translation>解压失败：%1</translation>
    </message>
    <message>
        <source>Extraction completed, but %1 entr%2 could not be moved</source>
        <translation>解压完成，但有 %1 个条目无法移动</translation>
    </message>
    <message>
        <source>Extraction completed</source>
        <translation>解压完成</translation>
    </message>
    <message>
        <source>tar does not support encryption; use 7z or zip instead</source>
        <translation>tar 不支持加密，请改用 7z 或 zip</translation>
    </message>
    <message>
        <source>Compression failed: %1</source>
        <translation>压缩失败：%1</translation>
    </message>
    <message>
        <source>Archive created: %1</source>
        <translation>压缩包已创建：%1</translation>
    </message>
    <message>
        <source>Cannot add files to compressed tar archives</source>
        <translation>无法向压缩的 tar 压缩包添加文件</translation>
    </message>
    <message>
        <source>Add files failed: %1</source>
        <translation>添加文件失败：%1</translation>
    </message>
    <message>
        <source>Files added to archive</source>
        <translation>文件已添加到压缩包</translation>
    </message>
    <message>
        <source>Cannot delete files from compressed tar archives</source>
        <translation>无法从压缩的 tar 压缩包删除文件</translation>
    </message>
    <message>
        <source>Delete files failed: %1</source>
        <translation>删除文件失败：%1</translation>
    </message>
    <message>
        <source>Files deleted from archive</source>
        <translation>文件已从压缩包删除</translation>
    </message>
    <message>
        <source>Archive test failed (corrupted)</source>
        <translation>压缩包测试失败（文件已损坏）</translation>
    </message>
    <message>
        <source>Archive integrity verified</source>
        <translation>压缩包完整性校验通过</translation>
    </message>
</context>
</TS>
