[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Version,

    [ValidateSet('x64-release', 'x64-debug')]
    [string]$Preset = 'x64-release',

    [ValidateSet('ninja', 'nmake', 'vs2022')]
    [string]$Generator = 'ninja',

    [string]$Exe
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$dist = Join-Path $root 'dist'

if (-not $Exe) {
    $candidates = @(
        (Join-Path $root "build/$Preset-$Generator/Release/TinyTheme.exe"),
        (Join-Path $root "build/$Preset-$Generator/TinyTheme.exe"),
        (Join-Path $root "build/$Preset/TinyTheme.exe")
    )
    $Exe = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Exe -or -not (Test-Path -LiteralPath $Exe)) {
    throw "找不到 TinyTheme.exe；请先构建，或用 -Exe 指定路径。"
}

$exeItem = Get-Item -LiteralPath $Exe
$fileVersion = $exeItem.VersionInfo.FileVersion
if ($fileVersion -ne $Version) {
    throw "版本不一致：exe 是 $fileVersion，参数是 $Version。先改 CMakeLists.txt 的 project(VERSION ...) 并重建。"
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
$stage = Join-Path $dist "stage-$Version"
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item -LiteralPath $exeItem.FullName -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'README.md') -Destination $stage
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') -Destination $stage

$zip = Join-Path $dist "TinyTheme-v$Version-x64.zip"
if (Test-Path -LiteralPath $zip) {
    Remove-Item -LiteralPath $zip -Force
}
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash
$sumFile = Join-Path $dist 'SHA256SUMS.txt'
"$hash  TinyTheme-v$Version-x64.zip" | Set-Content -LiteralPath $sumFile -Encoding ascii

Remove-Item -LiteralPath $stage -Recurse -Force

Write-Host "产物：$zip"
Write-Host "哈希：$hash"
Write-Host "校验：$sumFile"
