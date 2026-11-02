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
$sourceFiles = @($staged | Where-Object { $_ -match '\.(cpp|h)$' })
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
