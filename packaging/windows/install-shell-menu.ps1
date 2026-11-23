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

# "Compress to" submenu entries: registry id, label and the --compress token.
# “压缩到…”子菜单：注册表 id、显示名称与 --compress 参数。
$CompressFormats = @(
    @{ Id = '01zip';    Label = 'ZIP 压缩包 (.zip)';         Token = 'zip' },
    @{ Id = '02sevenz'; Label = '7z 压缩包 (.7z)';            Token = '7z' },
    @{ Id = '03targz';  Label = 'Gzip tar (.tar.gz)';         Token = 'tar.gz' },
    @{ Id = '04tarxz';  Label = 'Xz tar (.tar.xz)';           Token = 'tar.xz' },
    @{ Id = '05tarbz2'; Label = 'Bzip2 tar (.tar.bz2)';       Token = 'tar.bz2' },
    @{ Id = '06tar';    Label = 'Tar (.tar)';                 Token = 'tar' }
)

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

# "Add to <name>.zip" for one file or one folder.
# 为单个文件/文件夹提供“添加到同名 ZIP 压缩包”。
function New-AddToZipVerb([string]$BaseKey, [string]$TargetToken) {
    New-ShellVerb -BaseKey $BaseKey -Verb 'QArchiver.AddToZip' `
        -Label '添加到同名 ZIP 压缩包' `
        -Command ('"{0}" --compress zip "{1}"' -f $ExePath, $TargetToken)
}

# "Compress to..." submenu with the common formats and a custom dialog.
# “压缩到…”二级菜单：常见格式与自定义对话框。
function New-CompressToSubmenu([string]$BaseKey, [string]$TargetToken) {
    $key = Join-Path $BaseKey 'QArchiver.CompressTo'
    Remove-RegistryKey $key
    Set-RegistryValue $key 'MUIVerb' '压缩到...'
    Set-RegistryValue $key 'SubCommands' ''
    Set-RegistryValue $key 'Icon' "$ExePath,0"

    foreach ($format in $CompressFormats) {
        $verbKey = Join-Path $key ('shell\' + $format.Id)
        Set-RegistryValue $verbKey 'MUIVerb' $format.Label
        Set-RegistryValue (Join-Path $verbKey 'command') '(Default)' (
            '"{0}" --compress {1} "{2}"' -f $ExePath, $format.Token, $TargetToken)
    }

    $customKey = Join-Path $key 'shell\99custom'
    Set-RegistryValue $customKey 'MUIVerb' '自定义压缩选项...'
    Set-RegistryValue (Join-Path $customKey 'command') '(Default)' (
        '"{0}" --compress-dialog "{1}"' -f $ExePath, $TargetToken)
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
# ---------------------------------------------------------------------------
$fileBase = Join-Path $ClassesRoot '*\shell'
$directoryBase = Join-Path $ClassesRoot 'Directory\shell'
$backgroundBase = Join-Path $ClassesRoot 'Directory\Background\shell'

New-AddToZipVerb -BaseKey $fileBase -TargetToken '%1'
New-CompressToSubmenu -BaseKey $fileBase -TargetToken '%1'

New-AddToZipVerb -BaseKey $directoryBase -TargetToken '%1'
New-CompressToSubmenu -BaseKey $directoryBase -TargetToken '%1'

# Right-click on the empty area of a folder compresses that folder.
# 在文件夹空白处右击可压缩当前文件夹。
New-AddToZipVerb -BaseKey $backgroundBase -TargetToken '%V'
New-CompressToSubmenu -BaseKey $backgroundBase -TargetToken '%V'

Write-Host "QArchiver shell menu installed for $ExePath ($Scope scope)"
Write-Host "已注册右键菜单：打开/一键解压、添加到同名 ZIP、压缩到（7z/ZIP/tar 等）。"
