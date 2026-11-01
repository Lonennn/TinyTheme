<#
.SYNOPSIS
    用系统自带的 Chromium 浏览器（Edge / Chrome）把 res/*.svg 渲染成四档 PNG。

.DESCRIPTION
    不需要安装 Inkscape / ImageMagick：Windows 10/11 自带 Edge，直接以无头模式
    按目标像素尺寸截图即可。每档单独渲染，不做缩放。

    输出：<BuildDir>/<Name>-<size>.png，供 tools/pack-icon.ps1 打包。

.EXAMPLE
    pwsh -File tools/render-icons.ps1
#>
param(
    [string]$ResDir = 'res',
    [string]$BuildDir = 'res/build',
    [int[]]$Sizes = @(16, 20, 24, 32),
    [string[]]$Themes = @('light', 'dark'),
    [string]$Browser
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Find-Browser {
    param([string]$Override)
    if ($Override) {
        if (-not (Test-Path -LiteralPath $Override)) { throw "浏览器不存在：$Override" }
        return $Override
    }
    $candidates = @(
        "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe",
        "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
        "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
        "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path -LiteralPath $c) { return $c }
    }
    throw '未找到 Edge 或 Chrome；可用 -Browser 指定可执行文件路径。'
}

$browser = Find-Browser -Override $Browser
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$ResDir = (Resolve-Path -LiteralPath $ResDir).Path
$profileDir = Join-Path $BuildDir '_browser-profile'
$page = Join-Path $BuildDir '_render.html'

foreach ($theme in $Themes) {
    $svg = Join-Path $ResDir "icon-$theme.svg"
    if (-not (Test-Path -LiteralPath $svg)) { throw "缺少 $svg" }

    foreach ($size in $Sizes) {
        $html = @"
<!doctype html><meta charset="utf-8">
<style>
  html,body{margin:0;padding:0;background:transparent;overflow:hidden}
  img{display:block;width:${size}px;height:${size}px}
</style>
<img src="../icon-$theme.svg">
"@
        Set-Content -LiteralPath $page -Value $html -Encoding utf8

        $png = Join-Path $BuildDir "icon-$theme-$size.png"
        if (Test-Path -LiteralPath $png) { Remove-Item -LiteralPath $png -Force }

        $pageUrl = 'file:///' + ((Resolve-Path -LiteralPath $page).Path -replace '\\', '/')
        & $browser --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1 `
            --no-first-run --no-default-browser-check --disable-breakpad `
            --user-data-dir="$profileDir" --default-background-color=00000000 `
            --window-size="$size,$size" --screenshot="$png" $pageUrl 2>$null | Out-Null

        if (-not (Test-Path -LiteralPath $png)) { throw "渲染失败：$png" }
        $bmp = [System.Drawing.Bitmap]::new($png)
        $w = $bmp.Width
        $h = $bmp.Height
        $bmp.Dispose()
        if ($w -ne $size -or $h -ne $size) {
            throw "渲染尺寸不符：$png 实际 ${w}x${h}，期望 ${size}x${size}"
        }
        Write-Host "渲染 icon-$theme-$size.png"
    }
}

Remove-Item -LiteralPath $page -Force
Write-Host "完成：$BuildDir"
