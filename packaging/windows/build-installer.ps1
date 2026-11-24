# Build the QArchiver Windows installer from a portable ZIP package.
# 从便携 ZIP 包构建 QArchiver Windows 安装器。
#
#   .\build-installer.ps1 -ZipPath ..\..\QArchiver-2.3.1-windows-x64.zip
#
# Requires Inno Setup 6 (ISCC.exe).  CI installs it through Chocolatey when the
# runner image does not provide it.
# 需要 Inno Setup 6（ISCC.exe）。CI 在镜像未自带时通过 Chocolatey 安装。

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ZipPath,
    [string]$OutputDir = (Join-Path $PSScriptRoot '..\..\dist'),
    [ValidateSet('x64', 'arm64')][string]$Arch = 'x64',
    [string]$Version = ''
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $ZipPath)) {
    throw "Portable package not found: $ZipPath"
}
$zip = (Resolve-Path -LiteralPath $ZipPath).Path

if ([string]::IsNullOrWhiteSpace($Version)) {
    # Prefer the version encoded in QArchiver-<version>-windows-<arch>.zip,
    # which carries the release revision (e.g. 2.4.0-1).
    $zipName = [System.IO.Path]::GetFileNameWithoutExtension($zip)
    $zipVersion = [regex]::Match($zipName,
        '^QArchiver-(.+)-windows-(x64|arm64)$').Groups[1].Value
    if (-not [string]::IsNullOrWhiteSpace($zipVersion)) {
        $Version = $zipVersion
    } else {
        $cmakeFile = Join-Path $PSScriptRoot '..\..\CMakeLists.txt'
        $match = Select-String -Path $cmakeFile `
            -Pattern 'VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)' | Select-Object -First 1
        if (-not $match) {
            throw "Cannot read the project version from $cmakeFile"
        }
        $Version = $match.Matches[0].Groups[1].Value
    }
}

$dist = [System.IO.Path]::GetFullPath($OutputDir)
New-Item -ItemType Directory -Path $dist -Force | Out-Null

# Stage the portable payload so Inno Setup can copy it recursively.
# 展开便携包内容，供 Inno Setup 递归打包。
$stage = Join-Path $dist "installer-stage-$Arch"
Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
Expand-Archive -LiteralPath $zip -DestinationPath $stage -Force

$candidates = @($env:INNO_SETUP_ISCC)
$programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
if ($programFilesX86) {
    $candidates += (Join-Path $programFilesX86 'Inno Setup 6\ISCC.exe')
}
$candidates += (Join-Path $env:ProgramFiles 'Inno Setup 6\ISCC.exe')
$candidates += (Join-Path $env:LOCALAPPDATA 'Programs\Inno\ISCC.exe')
$candidates += (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe')
$iscc = $candidates | Where-Object {
    $_ -and (Test-Path -LiteralPath $_ -PathType Leaf)
} | Select-Object -First 1
if (-not $iscc) {
    throw ('Inno Setup 6 (ISCC.exe) was not found. Install it with ' +
        '"choco install innosetup -y" or set INNO_SETUP_ISCC.')
}

$VersionNumeric = $Version -replace '-', '.'
Write-Host "Building QArchiver $Version installer for $Arch"
Write-Host "  payload: $stage"
Write-Host "  output:  $dist"

$archAllowed = if ($Arch -eq 'arm64') { 'arm64' } else { 'x64compatible' }
& $iscc (Join-Path $PSScriptRoot 'QArchiver.iss') `
    "/DAppVersion=$Version" "/DAppVersionNumeric=$VersionNumeric" `
    "/DSourceDir=$stage" "/DOutputDir=$dist" "/DArch=$Arch" "/DArchAllowed=$archAllowed"
if ($LASTEXITCODE -ne 0) {
    throw "ISCC failed with exit code $LASTEXITCODE"
}

$setup = Join-Path $dist "QArchiver-$Version-windows-$Arch-setup.exe"
if (-not (Test-Path -LiteralPath $setup)) {
    throw "Installer was not created: $setup"
}

# Keep only the installer in dist; the staged tree is build output.
Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
Write-Host "Created $setup"
Write-Output $setup
