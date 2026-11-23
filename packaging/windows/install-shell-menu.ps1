# QArchiver Windows Explorer integration (current user, no admin required).
# Run this script from PowerShell after unzipping QArchiver.
# 以当前用户身份注册资源管理器右键菜单（无需管理员权限）。
#
#   powershell -ExecutionPolicy Bypass -File .\install-shell-menu.ps1
#
# Use -ExePath if QArchiver.exe is not next to this script.
# 如果 QArchiver.exe 不在脚本旁边，请用 -ExePath 指定路径。

param(
    [string]$ExePath = (Join-Path $PSScriptRoot 'QArchiver.exe')
)

$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $ExePath)) {
    throw "QArchiver.exe not found: $ExePath"
}
$ExePath = (Resolve-Path -LiteralPath $ExePath).Path
$Extensions = @('.zip', '.7z', '.rar', '.tar', '.gz', '.bz2', '.xz',
                '.tgz', '.txz', '.tbz2', '.tar.gz', '.tar.xz', '.tar.bz2')

foreach ($ext in $Extensions) {
    $base = "HKCU:\Software\Classes\SystemFileAssociations\$ext\shell"
    $open = Join-Path $base 'QArchiver.Open'
    New-Item -Path "$open\command" -Force | Out-Null
    Set-ItemProperty -Path $open -Name '(Default)' -Value '用 小Q解压 打开'
    Set-ItemProperty -Path $open -Name 'Icon' -Value "$ExePath,0"
    Set-ItemProperty -Path "$open\command" -Name '(Default)' `
        -Value ('"{0}" "%1"' -f $ExePath)

    $one = Join-Path $base 'QArchiver.OneClick'
    New-Item -Path "$one\command" -Force | Out-Null
    Set-ItemProperty -Path $one -Name '(Default)' -Value '一键解压'
    Set-ItemProperty -Path $one -Name 'Icon' -Value "$ExePath,0"
    Set-ItemProperty -Path "$one\command" -Name '(Default)' `
        -Value ('"{0}" --oneclick "%1"' -f $ExePath)
}

Write-Host "QArchiver shell menu installed for $ExePath"
Write-Host "已为当前用户注册“用 小Q解压 打开”和“一键解压”右键菜单。"
