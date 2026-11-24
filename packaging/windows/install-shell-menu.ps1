# QArchiver Windows Explorer integration.
# 小Q解压 Windows 资源管理器右键菜单。
#
# Portable package (current user, no admin required):
#   便携包（当前用户，无需管理员）：
#   powershell -ExecutionPolicy Bypass -File .\install-shell-menu.ps1
#
# All users (requires an elevated PowerShell):
#   所有用户（需要管理员权限的 PowerShell）：
#   powershell -ExecutionPolicy Bypass -File .\install-shell-menu.ps1 -Scope Machine
#
# Use -ExePath if QArchiver.exe is not next to this script.
# 如果 QArchiver.exe 不在脚本旁边，可用 -ExePath 指定路径。
#
# The menus installed here mirror the [Registry] section of QArchiver.iss.
# Keep both definitions in sync when adding a format.
# 此处安装的菜单与 QArchiver.iss 的 [Registry] 段一致；
# 新增格式时请同步修改两处。

param(
    [string]$ExePath = (Join-Path $PSScriptRoot 'QArchiver.exe'),
    [ValidateSet('User', 'Machine')][string]$Scope = 'User'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "QArchiver.exe not found: $ExePath"
}
$ExePath = (Resolve-Path -LiteralPath $ExePath).Path

$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
if ($Scope -eq 'Machine' -and -not $isAdmin) {
    throw "Scope 'Machine' requires an elevated PowerShell window."
}

$ClassesRoot = if ($Scope -eq 'Machine') {
    'HKLM:\SOFTWARE\Classes'
} else {
    'HKCU:\Software\Classes'
}

# Archive types that get "open with QArchiver" and "one-click extract".
# 获得“用 小Q解压 打开”和“一键解压”的压缩包类型。
$Extensions = @('.zip', '.7z', '.rar', '.tar', '.gz', '.bz2', '.xz',
                '.tgz', '.txz', '.tbz2', '.tar.gz', '.tar.xz', '.tar.bz2')

function Remove-RegistryKey([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

function Set-RegistryValue([string]$Path, [string]$Name, [string]$Value) {
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -Path $Path -Force | Out-Null
    }
    Set-ItemProperty -LiteralPath $Path -Name $Name -Value $Value
}

function New-ShellVerb([string]$BaseKey, [string]$Verb, [string]$Label,
                       [string]$Command, [string]$Icon = "$ExePath,0") {
    $key = Join-Path $BaseKey $Verb
    Remove-RegistryKey $key
    Set-RegistryValue $key '(Default)' $Label
    Set-RegistryValue $key 'MUIVerb' $Label
    Set-RegistryValue $key 'Icon' $Icon
    Set-RegistryValue (Join-Path $key 'command') '(Default)' $Command
}

# ---------------------------------------------------------------------------
# Extract commands for archive files / 压缩包解压命令
# ---------------------------------------------------------------------------
foreach ($ext in $Extensions) {
    $base = Join-Path $ClassesRoot "SystemFileAssociations\$ext\shell"
    New-ShellVerb -BaseKey $base -Verb 'QArchiver.Open' `
        -Label '用 小Q解压 打开' -Command ('"{0}" "%1"' -f $ExePath)
    New-ShellVerb -BaseKey $base -Verb 'QArchiver.OneClick' `
        -Label '一键解压' -Command ('"{0}" --oneclick "%1"' -f $ExePath)
}

# ---------------------------------------------------------------------------
# Compress commands for files and folders / 文件与文件夹的压缩命令
#
# These come from QArchiverShell.dll so the label can contain the selected
# file name and archive files can be excluded dynamically.
# 文件/文件夹压缩命令由 QArchiverShell.dll 提供，文案可包含文件名，并能
# 动态排除已经是压缩包的文件。
# ---------------------------------------------------------------------------
$shellDll = Join-Path (Split-Path -Parent $ExePath) 'QArchiverShell.dll'
if (-not (Test-Path -LiteralPath $shellDll)) {
    # Installer payloads use a versioned filename so an in-place upgrade is not
    # blocked by Explorer still mapping the previous DLL.
    # 安装器使用带版本号的文件名，避免 Explorer 占用旧 DLL 导致升级失败。
    # Sorting by name would pick 2.4.9 over 2.4.10, so compare the parsed
    # version instead.
    # 按名称排序会把 2.4.10 排到 2.4.9 前面，因此改为比较解析后的版本号。
    $versioned = Get-ChildItem -LiteralPath (Split-Path -Parent $ExePath) `
        -Filter 'QArchiverShell-*.dll' -File |
        Sort-Object -Descending {
            $raw = $_.BaseName -replace '^QArchiverShell-', ''
            $parsed = $null
            if ([version]::TryParse($raw, [ref]$parsed)) { $parsed } else { [version]'0.0' }
        } | Select-Object -First 1
    if ($versioned) {
        $shellDll = $versioned.FullName
    }
}
if (-not (Test-Path -LiteralPath $shellDll)) {
    throw "QArchiverShell.dll not found next to QArchiver.exe: $shellDll"
}

# A machine-wide install must not be shadowed by an older per-user
# registration of the same CLSID.
# 机器级安装前清理旧的用户级 CLSID 注册，避免加载旧 DLL。
if ($Scope -eq 'Machine') {
    $clsid = '{5F3B9C2A-7E4D-4A1B-9C6D-2B8E1F4A7C30}'
    Remove-RegistryKey "HKCU:\Software\Classes\CLSID\$clsid"
    Remove-RegistryKey 'HKCU:\Software\Classes\*\shellex\ContextMenuHandlers\QArchiver'
    Remove-RegistryKey 'HKCU:\Software\Classes\Directory\shellex\ContextMenuHandlers\QArchiver'
    Remove-RegistryKey 'HKCU:\Software\Classes\Directory\Background\shellex\ContextMenuHandlers\QArchiver'
}

# Remove static compress verbs installed by QArchiver <= 2.2.0; the shell
# extension replaces them with dynamic labels.
# 删除 2.2.0 及更早版本写入的静态压缩菜单；改由扩展提供动态文案。
foreach ($base in @(
        (Join-Path $ClassesRoot '*\shell'),
        (Join-Path $ClassesRoot 'Directory\shell'),
        (Join-Path $ClassesRoot 'Directory\Background\shell'))) {
    foreach ($verb in @('QArchiver.AddToZip', 'QArchiver.CompressTo')) {
        Remove-RegistryKey (Join-Path $base $verb)
    }
}

$regsvr = if ([Environment]::Is64BitProcess) {
    Join-Path $env:SystemRoot 'System32\regsvr32.exe'
} else {
    Join-Path $env:SystemRoot 'Sysnative\regsvr32.exe'
}
$regsvrProcess = Start-Process -FilePath $regsvr `
    -ArgumentList @('/s', ('"{0}"' -f $shellDll)) `
    -Wait -PassThru -WindowStyle Hidden
if ($regsvrProcess.ExitCode -ne 0) {
    throw "regsvr32 failed to register $shellDll (exit code $($regsvrProcess.ExitCode))."
}

Write-Host "QArchiver shell menu installed for $ExePath ($Scope scope)"
Write-Host "已注册右键菜单：打开/一键解压，以及动态“添加到 文件名.zip”和“压缩到…”。"
