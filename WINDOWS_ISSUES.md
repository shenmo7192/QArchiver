# QArchiver Windows 问题发现记录

> 状态：**仅记录，未修改代码**。  
> 适用范围：Windows 资源管理器右键菜单、文件关联、安装器与便携包脚本。  
> 审查方式：源码静态审查（Linux 环境），**尚未在 Windows 实机逐项复现**。  
> 记录时间：2026-11-23（审查时项目版本 0.1.3）。

本文只记录 Windows 相关问题；跨平台问题（如 `FormatRegistry` 后缀误判、现代布局图标缺失、Linux `mimeapps.list` 写入等）不在本文范围内。

---

## 严重程度说明

| 级别 | 含义 |
|---|---|
| P1 | 会导致功能错误、注册残留或明显的用户可见问题，建议优先处理 |
| P2 | 特定场景下出错，或维护成本/行为不一致 |
| P3 | 体验、文档、可维护性问题，可随后处理 |

---

## P1-1 多选时只检查第一个文件，菜单显示与压缩行为不一致

**位置**

- `src/windows/QArchiverShell.cpp:250-263`
- `src/windows/QArchiverShell.cpp:133-153`
- `packaging/windows/README.md:87-91`

**现象**

`QueryContextMenu()` 只用 `targets.front()` 判断是否为目录/压缩包，并只用第一个文件名生成菜单文案：

```cpp
const DWORD attributes = GetFileAttributesW(targets.front().c_str());
...
if (!isDirectory && hasArchiveSuffix(targets.front())) {
    return 0;
}
const std::wstring zipName =
    baseName(targets.front(), isDirectory) + L".zip";
```

但 `runCompression()` 在多选且非“自定义压缩选项”时，会为 **每个 target 各启动一个进程**：

```cpp
for (const std::wstring &path : paths) {
    launch(std::wstring(L"--compress ") + format + L" " + quote(path));
}
```

由此产生以下不一致：

1. 选中“压缩包 + 普通文件”时，如果第一个是压缩包，整个菜单被隐藏，普通文件也无法压缩；
2. 如果第一个是普通文件，菜单会显示，但第二个压缩包也会被再次压缩；
3. 多选时菜单文案只显示第一个文件的 `名称.zip`，与实际“每项各生成一个压缩包”的行为不符；
4. 第一个是目录时，`isDirectory` 只代表第一个目标，后续文件的目录属性未参与判断。

`packaging/windows/README.md` 写的是“已经是压缩包的文件不显示这些压缩命令”，与多选行为不一致。

**建议**

- 在 `QueryContextMenu()` 中先过滤 `targets`：
  - 去掉已经是压缩包的文件；
  - 或至少在所有目标都是压缩包时隐藏菜单。
- 用过滤后的列表判断是否为空、生成文案，并把过滤后的列表传给 `runCompression()`。
- 文案可按数量生成，例如“添加到 3 个压缩包…”，或使用第一个非压缩包文件名。
- `InvokeCommand()` 也应使用同一套过滤逻辑，避免菜单显示与实际执行目标不一致。

---

## P1-2 PowerShell 的 `-Scope` 与 DLL 的实际注册根不一致

**位置**

- `packaging/windows/install-shell-menu.ps1:39-43`
- `packaging/windows/install-shell-menu.ps1:109-119`
- `src/windows/QArchiverShell.cpp:383-386`
- `src/windows/QArchiverShell.cpp:453-481`

**现象**

安装脚本按显式 `-Scope` 选择注册表根：

```powershell
$ClassesRoot = if ($Scope -eq 'Machine') {
    'HKLM:\SOFTWARE\Classes'
} else {
    'HKCU:\Software\Classes'
}
```

静态的“打开 / 一键解压”动词写入该根；但 `regsvr32` 调用没有传入 scope，DLL 内部用 `IsUserAnAdmin()` 自行判断：

```cpp
HKEY classesRoot()
{
    return IsUserAnAdmin() ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}
```

因此：

- 非管理员执行默认 `-Scope User`：脚本写 HKCU，DLL 也写 HKCU，正常；
- 管理员执行 `-Scope Machine`：脚本写 HKLM，DLL 也写 HKLM，正常；
- **在提权的 PowerShell 中执行 `-Scope User`**：静态动词写 HKCU，但 DLL 会把 CLSID 和 `ContextMenuHandlers` 写到 HKLM，形成“半用户级、半机器级”注册；
- 之后用默认 `-Scope User`（非提权）卸载时，DLL 只删除 HKCU 的 CLSID，HKLM 下的残留不会被清理。

**建议**

- 给 DLL 增加显式的注册作用域参数（例如自定义导出 `DllRegisterServerUser` / 环境变量 / 命令行参数），由脚本传入；
- 或让脚本根据实际 `$Scope` 固定调用对应的注册逻辑，不再依赖 `IsUserAnAdmin()`；
- 卸载时同时检查并清理 HKCU 与 HKLM 两处，或至少检测另一处残留并提示。

---

## P2-1 `DllRegisterServer` 总是向 HKLM 写入 Approved 列表

**位置**

- `src/windows/QArchiverShell.cpp:472-478`
- `src/windows/QArchiverShell.cpp:492-495`

**现象**

无论实际注册根是 HKCU 还是 HKLM，DLL 都固定写：

```cpp
setString(HKEY_LOCAL_MACHINE,
          L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell "
          L"Extensions\\Approved",
          kClsidString, kApprovedName);
```

- 普通用户注册时写 HKLM 会失败，代码忽略返回值；
- 提权但按用户级注册时，会把只存在于 HKCU 的 CLSID 写进机器级 Approved 列表；
- 卸载时也只从 HKLM 删除，用户级注册不会清理对应项。

**建议**

- Approved 列表跟随 `classesRoot()` 使用同一个根；
- 只在 HKLM 注册时写 HKLM Approved；
- 注册/卸载前检查权限，失败时至少记录日志。

---

## P2-2 卸载脚本不检查 `regsvr32` 退出码，且 DLL 路径来源与安装脚本不同

**位置**

- `packaging/windows/uninstall-shell-menu.ps1:58-68`
- `packaging/windows/install-shell-menu.ps1:92-119`

**现象**

安装脚本会检查 `regsvr32` 的退出码：

```powershell
if ($regsvrProcess.ExitCode -ne 0) {
    throw "regsvr32 failed ..."
}
```

卸载脚本却没有检查：

```powershell
Start-Process -FilePath $regsvr `
    -ArgumentList @('/u', '/s', ('"{0}"' -f $shellDll)) `
    -Wait -WindowStyle Hidden
```

最后仍然输出“已移除 QArchiver 右键菜单”。如果 DLL 被占用、权限不足或路径错误，用户会看到成功提示，但残留仍然存在。

另外：

- 安装脚本支持 `-ExePath`，并按 `QArchiver.exe` 所在目录找 `QArchiverShell.dll`；
- 卸载脚本没有 `-ExePath` 参数，固定使用 `$PSScriptRoot`，无法处理“脚本与安装目录分离”的场景。

**建议**

- 卸载脚本检查 `ExitCode`，非 0 时抛出错误或至少输出警告；
- 增加 `-ExePath`（或 `-DllPath`）参数，与安装脚本保持一致；
- 清理静态动词和 DLL 注册分别报告结果。

---

## P2-3 安装器写 HKCU 关联与卸载器清理的账户可能不是同一个

**位置**

- `packaging/windows/QArchiver.iss:88-95`
- `src/utils/FileAssociations.cpp:99-201`

**现象**

安装器注册文件关联时使用 `runasoriginaluser`：

```ini
Filename: "{app}\{#AppExeName}"; Parameters: "--register-associations";
    Flags: runhidden waituntilterminated runasoriginaluser; Tasks: associate;
```

`FileAssociations` 的 Windows 实现固定写 `HKEY_CURRENT_USER`。因此关联写入的是“启动安装程序的原始用户”的 HKCU。

卸载时：

```ini
Filename: "{app}\{#AppExeName}"; Parameters: "--unregister-associations";
    Flags: runhidden waituntilterminated;
```

没有 `runasoriginaluser`，卸载器以提权账户运行。若安装时是标准用户通过“管理员凭据”提权（over-the-shoulder elevation），卸载时清理的可能是管理员账户的 HKCU，而原始标准用户的关联仍然保留。

**建议**

- 卸载关联也使用 `runasoriginaluser`，或提供“为所有用户/当前用户清理”的明确选项；
- 或者记录安装时写入的用户 SID，卸载时按记录清理；
- 至少在文档中说明“每用户关联”在跨账户安装/卸载时的限制。

---

## P2-4 Windows 各处的压缩包后缀列表不一致

**位置**

- `src/windows/QArchiverShell.cpp:58-75`（`hasArchiveSuffix`）
- `packaging/windows/install-shell-menu.ps1:47-48`、`31-32`（卸载）
- `packaging/windows/QArchiver.iss:98-114`
- `src/utils/FileAssociations.cpp:26-38`
- `src/common/FormatRegistry.cpp:230-260`（跨平台后缀真值表）

**现象**

当前至少存在四份 Windows 相关后缀列表，内容不一致：

| 列表 | 包含 | 缺失/差异 |
|---|---|---|
| `QArchiverShell::hasArchiveSuffix` | `.zip/.7z/.rar/.tar/.gz/.bz2/.xz/.tgz/.txz/.tbz2/.tar.*/.zst/.cab/.iso` | 不含 `.7z.001`、`.zip.001`、`.partN.rar`、`.r00` |
| `install-shell-menu.ps1` / `uninstall-shell-menu.ps1` | 13 种常见后缀 | 不含 `.tar.zst`、`.zst` |
| `QArchiver.iss` (`ArchiveExtensions`) | 13 种常见后缀 | 不含 `.tar.zst`、`.zst` |
| `FileAssociations::formatExtensions` | 13 种常见后缀 | 不含 `.tar.zst`、`.zst` |

可能结果：

- 右键选中 `data.7z.001`、`data.part1.rar`、`data.r00` 时，`hasArchiveSuffix` 认为不是压缩包，仍显示“添加到…/压缩到…”，用户可能对分卷再次压缩；
- `.tar.zst` / `.zst` 在 QArchiver 内可打开，但 Windows 安装器/便携脚本不注册“打开 / 一键解压”动词，也不注册文件关联；
- `.cab` / `.iso` 只出现在 shell 的“排除压缩”列表中，不出现在安装器的打开/解压动词中，行为含义不统一。

**建议**

- 以 `FormatRegistry` 为唯一事实来源，生成或集中维护 Windows 集成列表；
- 至少让 `QArchiverShell` 复用 `isArchivePath()` / `isSplitVolume()` 的规则；
- 明确 `.tar.zst` / `.zst` / `.cab` / `.iso` 是否属于 Windows 关联范围，并同步四处列表和文档。

---

## P2-5 压缩进程启动失败时没有任何反馈

**位置**

- `src/windows/QArchiverShell.cpp:110-153`

**现象**

`launch()` 返回 `bool`，但 `runCompression()` 完全忽略：

```cpp
for (const std::wstring &path : paths) {
    launch(std::wstring(L"--compress ") + format + L" " + quote(path));
}
```

如果 `QArchiver.exe` 被移动/删除、DLL 与 exe 不再同目录，或 `CreateProcessW` 失败，用户点击右键菜单后什么也不会发生，也没有日志。

此外，多选非自定义压缩时会为每个文件启动一个进程；选中大量文件时可能瞬间启动大量 QArchiver 进程。

**建议**

- 检查 `launch()` 返回值；失败时用 `MessageBoxW` 或写入日志/事件日志提示；
- 评估是否改为一次启动并传入多个目标（需要先明确“每项单独压缩”与“合并压缩”的产品语义）；
- 若保留每项一个进程，至少限制并发或按顺序等待，避免进程风暴。

---

## P3-1 菜单文案硬编码中文，但安装器提供英文界面

**位置**

- `src/windows/QArchiverShell.cpp:263, 268-277`
- `packaging/windows/install-shell-menu.ps1:78-81`
- `packaging/windows/QArchiver.iss:70-71, 137-140`

**现象**

资源管理器菜单、静态动词和任务描述使用硬编码中文；安装器同时提供 `english` 与 `chinesesimplified` 两种语言。选择英文安装界面的用户仍会看到中文右键菜单和中文任务说明。

**建议**

- 使用 MUI 动词/资源 DLL 提供中英文文案；
- 或至少为英文安装场景提供英文菜单资源；
- 若产品决定只提供中文菜单，请在 README 中明确说明。

---

## P3-2 脚本/安装器注释与结构已和实际实现不一致

**位置**

- `packaging/windows/install-shell-menu.ps1:15-18`
- `packaging/windows/QArchiver.iss:14-17, 143-147, 176-205`
- `packaging/windows/uninstall-shell-menu.ps1:47-54`

**现象**

- `install-shell-menu.ps1` 仍写着“与 QArchiver.iss 的 `[Registry]` 段一致”，但 `QArchiver.iss` 已经没有 `[Registry]` 段，静态动词现在写在 `[Code]` 中；
- `QArchiver.iss` 头部仍说“此处写入的资源管理器菜单与 install-shell-menu.ps1 一致”，但压缩菜单实际由 `QArchiverShell.dll` 动态提供；
- `QArchiver.iss` 的 `CurStepChanged` 与 `UnregisterShellMenu` 重复删除旧版静态动词，属于迁移代码，但两处列表重复，后续修改容易漏改。

**建议**

- 更新注释，明确“静态解压动词在 `[Code]` / 脚本中，压缩动词由 shell DLL 提供”；
- 把旧版静态动词的清理列表提取为常量或统一函数；
- 在 README 中画一张“安装器 / 便携脚本 / DLL 各自负责什么”的小表。

---

## P3-3 Windows 注册失败时没有错误信息

**位置**

- `src/utils/FileAssociations.cpp:335-376`

**现象**

Windows 分支使用 `Q_UNUSED(error)`；任一 `RegCreateKeyExW` / `RegSetValueExW` 失败时直接返回 `false`，但没有设置 `*error`。设置对话框或其他调用方可能只能显示空错误/通用错误。

**建议**

- 与 Linux 分支保持一致，失败时设置具体扩展名和原因；
- 至少记录 `GetLastError()`。

---

## 建议的实机验证清单

后续处理时建议至少覆盖：

1. 标准用户 + 管理员账户分别安装/卸载，检查 HKCU/HKLM 残留；
2. 提权 PowerShell 执行 `install-shell-menu.ps1 -Scope User`，再普通权限卸载，检查 HKLM 残留；
3. 多选“压缩包 + 普通文件”“多个压缩包”“普通文件 + 文件夹”，检查菜单文案与实际输出；
4. 选中 `data.7z.001`、`data.zip.001`、`data.part1.rar`、`data.r00`，确认是否出现压缩菜单；
5. 安装后移动 `QArchiver.exe` 或删除 DLL，点击右键菜单，确认是否有错误提示；
6. 英文安装器安装后检查右键菜单和任务说明语言；
7. 选中 `.tar.zst` / `.zst`，确认是否注册“打开 / 一键解压”和文件关联。
