#Requires -Version 7.0
# 提交前的本地检查：只读暂存区、只处理本次提交的文件，目标两秒内跑完。
# 钩子能被 git commit --no-verify 绕过，所以格式、构建、单测在 CI 里都有一份独立兜底。
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (& git rev-parse --show-toplevel).Trim()
Set-Location $repoRoot

$staged = @((& git diff --cached --name-only --diff-filter=ACMR -z) -split "`0" | Where-Object { $_ })
if ($staged.Count -eq 0)
{
    exit 0
}

$failures = [System.Collections.Generic.List[string]]::new()

# ---- 格式：clang-format 直接修好并重新暂存 ----
# third_party 与 CI 的格式检查保持一致，不参与；超大源文件跳过（clang-format 会在
# 这类文件上卡住，而正常源文件到不了这个体积）。
$sourceFiles = @($staged | Where-Object { $_ -match '\.(cpp|h)$' -and $_ -notmatch '(^|/)third_party/' })
if ($sourceFiles.Count -gt 0)
{
    if (-not (Get-Command clang-format -ErrorAction SilentlyContinue))
    {
        Write-Host 'pre-commit: 未找到 clang-format，跳过格式检查' -ForegroundColor DarkYellow
    }
    else
    {
        $reformatted = @()
        foreach ($file in $sourceFiles)
        {
            if ((Get-Item -LiteralPath $file -ErrorAction SilentlyContinue).Length -gt 512KB)
            {
                Write-Host "pre-commit: $file 超过 512 KB，跳过格式检查" -ForegroundColor DarkYellow
                continue
            }
            # 工作区与暂存内容不一致时，-i 会把未暂存的改动一起搅进这次提交。
            if (@(& git diff --name-only -- $file).Count -gt 0)
            {
                $failures.Add("$file 有未暂存的改动，先 git add 或 git stash 再提交")
                continue
            }
            & clang-format --style=file --dry-run -Werror $file 2>$null
            if ($LASTEXITCODE -eq 0)
            {
                continue
            }
            & clang-format --style=file -i $file
            & git add -- $file
            $reformatted += $file
        }
        if ($reformatted.Count -gt 0)
        {
            Write-Host ("pre-commit: 已格式化并重新暂存 " + ($reformatted -join ', ')) -ForegroundColor DarkGray
        }
    }
}

# ---- 行尾空白与冲突标记 ----
# --check 只看本次新增的行：行尾空白、空白行末、空格缩进，以及没清理的冲突标记。
$whitespace = & git diff --cached --check 2>&1
if ($LASTEXITCODE -ne 0)
{
    foreach ($line in @($whitespace))
    {
        if ($line)
        {
            $failures.Add([string]$line)
        }
    }
}

# ---- 编码与换行 ----
# 约定源文件 UTF-8 无 BOM；BOM 会让 diff、编译诊断与部分工具显示出多余的字符。
$binary = '\.(ico|png|jpg|jpeg|gif|zip|exe|dll|lib|pdb|res|obj)$'
foreach ($file in @($staged | Where-Object { $_ -notmatch $binary }))
{
    if (-not (Test-Path -LiteralPath $file))
    {
        continue
    }
    $head = @(Get-Content -LiteralPath $file -AsByteStream -TotalCount 3)
    if ($head.Count -eq 3 -and $head[0] -eq 0xEF -and $head[1] -eq 0xBB -and $head[2] -eq 0xBF)
    {
        $failures.Add("$file 带 UTF-8 BOM，按约定应去掉")
    }
}

# 索引里出现 CR：`.gitattributes` 声明了 * text=auto eol=lf，正常不该有；
# 真有说明某个路径被标成了 binary 或 -text。查索引而不是工作区，避免编辑器换行风格误报。
$crlf = & git grep --cached -I -l -P '\r' -- $staged 2>$null
if ($LASTEXITCODE -eq 0)
{
    foreach ($line in @($crlf))
    {
        if ($line)
        {
            $failures.Add("$line 在索引里是 CRLF，应统一 LF")
        }
    }
}

# ---- 体积与二进制 ----
# 仓库里最大的文件是 354 KB 的 doctest.h，图标都在 50 KB 以内；到这里基本是误提交。
$maxBytes = 1MB
$forbidden = '\.(exe|dll|obj|lib|pdb|res|ilk|exp|zip|7z|msi|msix|nupkg)$'
$size = [long]0
foreach ($file in $staged)
{
    $raw = (& git cat-file -s ":$file" | Select-Object -First 1)
    if ([long]::TryParse("$raw", [ref]$size) -and $size -gt $maxBytes)
    {
        $failures.Add(('{0} 有 {1:N1} MiB，超过 1 MiB 上限，确认不是误提交' -f $file, ($size / 1MB)))
    }
    if ($file -match $forbidden)
    {
        $failures.Add("$file 是构建产物或压缩包，不应入库")
    }
}

if ($failures.Count -gt 0)
{
    Write-Host ''
    foreach ($failure in $failures)
    {
        Write-Host "pre-commit: $failure" -ForegroundColor Yellow
    }
    exit 1
}

exit 0
