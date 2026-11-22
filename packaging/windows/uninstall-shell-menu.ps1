# Remove the QArchiver Windows Explorer integration for the current user.
# 移除当前用户的 QArchiver 资源管理器右键菜单。

$ErrorActionPreference = 'Stop'
$Extensions = @('.zip', '.7z', '.rar', '.tar', '.gz', '.bz2', '.xz',
                '.tgz', '.txz', '.tbz2', '.tar.gz', '.tar.xz', '.tar.bz2')

foreach ($ext in $Extensions) {
    $base = "HKCU:\Software\Classes\SystemFileAssociations\$ext\shell"
    foreach ($key in @('QArchiver.Open', 'QArchiver.OneClick')) {
        $path = Join-Path $base $key
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }
}

Write-Host "QArchiver shell menu removed."
Write-Host "已移除当前用户的 QArchiver 右键菜单。"
