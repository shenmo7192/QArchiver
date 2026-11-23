# Remove the QArchiver Windows Explorer integration.
# 移除 QArchiver 资源管理器右键菜单。
#
# Current user (portable package default):
#   当前用户（便携包默认）：
#   powershell -ExecutionPolicy Bypass -File .\uninstall-shell-menu.ps1
#
# All users (installer / elevated shell):
#   所有用户（安装器/管理员 PowerShell）：
#   powershell -ExecutionPolicy Bypass -File .\uninstall-shell-menu.ps1 -Scope Machine

param(
    [ValidateSet('User', 'Machine')][string]$Scope = 'User'
)

$ErrorActionPreference = 'Stop'

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

$Extensions = @('.zip', '.7z', '.rar', '.tar', '.gz', '.bz2', '.xz',
                '.tgz', '.txz', '.tbz2', '.tar.gz', '.tar.xz', '.tar.bz2')

function Remove-RegistryKey([string]$Path) {
    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }
}

foreach ($ext in $Extensions) {
    $base = Join-Path $ClassesRoot "SystemFileAssociations\$ext\shell"
    foreach ($verb in @('QArchiver.Open', 'QArchiver.OneClick')) {
        Remove-RegistryKey (Join-Path $base $verb)
    }
}

foreach ($base in @(
        (Join-Path $ClassesRoot '*\shell'),
        (Join-Path $ClassesRoot 'Directory\shell'),
        (Join-Path $ClassesRoot 'Directory\Background\shell'))) {
    foreach ($verb in @('QArchiver.AddToZip', 'QArchiver.CompressTo')) {
        Remove-RegistryKey (Join-Path $base $verb)
    }
}

Write-Host "QArchiver shell menu removed ($Scope scope)."
Write-Host "已移除 QArchiver 右键菜单。"
