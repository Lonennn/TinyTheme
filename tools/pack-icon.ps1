<#
.SYNOPSIS
    把四档 PNG 打包成多尺寸 ICO。

.DESCRIPTION
    读取 <BuildDir>/<Name>-<size>.png，按 ICO 规范写成 <OutDir>/<Name>.ico。
    每个条目用 32 位 BGRA 的 BMP（DIB）+ AND 掩码承载，不内嵌 PNG，
    以便托盘图标在 16~32 px 下的兼容性最好。

.EXAMPLE
    pwsh -File tools/pack-icon.ps1 -Name icon-light

.EXAMPLE
    pwsh -File tools/pack-icon.ps1 -Name icon-dark -BuildDir res/build -OutDir res
#>
param(
    [Parameter(Mandatory)][string]$Name,
    [string]$BuildDir = 'res/build',
    [string]$OutDir = 'res',
    [int[]]$Sizes = @(16, 20, 24, 32)
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Get-IconEntry {
    param([string]$Path)
    $bmp = [System.Drawing.Bitmap]::new($Path)
    $w = $bmp.Width
    $h = $bmp.Height
    $rect = [System.Drawing.Rectangle]::new(0, 0, $w, $h)
    $locked = $bmp.LockBits($rect,
        [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $stride = $locked.Stride
    $pixels = New-Object byte[] ($stride * $h)
    [System.Runtime.InteropServices.Marshal]::Copy($locked.Scan0, $pixels, 0, $pixels.Length)
    $bmp.UnlockBits($locked)
    $bmp.Dispose()

    $maskStride = [int]([math]::Ceiling($w / 32.0) * 4)
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)

    # BITMAPINFOHEADER：biHeight 是图像高度的 2 倍（颜色位图 + AND 掩码）
    $bw.Write([uint32]40)
    $bw.Write([int32]$w)
    $bw.Write([int32]($h * 2))
    $bw.Write([uint16]1)
    $bw.Write([uint16]32)
    $bw.Write([uint32]0)
    $bw.Write([uint32]($w * $h * 4 + $maskStride * $h))
    $bw.Write([int32]0)
    $bw.Write([int32]0)
    $bw.Write([uint32]0)
    $bw.Write([uint32]0)

    # 颜色位图：BGRA，自下而上
    for ($y = $h - 1; $y -ge 0; $y--) { $bw.Write($pixels, $y * $stride, $w * 4) }

    # AND 掩码：1 = 透明
    for ($y = $h - 1; $y -ge 0; $y--) {
        $row = New-Object byte[] $maskStride
        for ($x = 0; $x -lt $w; $x++) {
            if ($pixels[$y * $stride + $x * 4 + 3] -eq 0) {
                $i = [int][math]::Floor($x / 8)
                $row[$i] = $row[$i] -bor (0x80 -shr ($x % 8))
            }
        }
        $bw.Write($row, 0, $maskStride)
    }

    $bw.Flush()
    $bw.Dispose()
    $data = $ms.ToArray()
    $ms.Dispose()
    [pscustomobject]@{ Width = $w; Height = $h; Data = $data }
}

$entries = @(foreach ($s in $Sizes) {
    $png = Join-Path $BuildDir "$Name-$s.png"
    if (-not (Test-Path -LiteralPath $png)) { throw "缺少 $png" }
    Get-IconEntry -Path $png
})

$icoPath = Join-Path $OutDir "$Name.ico"
$stream = [System.IO.File]::Create($icoPath)
$iw = New-Object System.IO.BinaryWriter($stream)
$iw.Write([uint16]0)
$iw.Write([uint16]1)
$iw.Write([uint16]$entries.Count)
$offset = 6 + 16 * $entries.Count
foreach ($e in $entries) {
    $iw.Write([byte]($(if ($e.Width -ge 256) { 0 } else { $e.Width })))
    $iw.Write([byte]($(if ($e.Height -ge 256) { 0 } else { $e.Height })))
    $iw.Write([byte]0)
    $iw.Write([byte]0)
    $iw.Write([uint16]1)
    $iw.Write([uint16]32)
    $iw.Write([uint32]$e.Data.Length)
    $iw.Write([uint32]$offset)
    $offset += $e.Data.Length
}
foreach ($e in $entries) { $iw.Write($e.Data) }
$iw.Dispose()

Write-Host "生成 $icoPath（$($entries.Count) 个尺寸）"
