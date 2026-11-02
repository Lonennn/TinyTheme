#Requires -Version 7.0
# 校验提交标题是否符合 Conventional Commits：type(scope): subject，标题不超过 72 字符。
# 只拦明显不合规的输入，不判断语义；git commit --no-verify 可以绕过。
param(
    [Parameter(Mandatory = $true)]
    [string]$MessageFile
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $MessageFile))
{
    exit 0
}

# COMMIT_EDITMSG 里带着以 # 开头的注释与模板，先滤掉再看第一行。
$lines = @(Get-Content -LiteralPath $MessageFile -Encoding utf8)
$subject = @($lines | Where-Object { $_.Trim() -ne '' -and -not $_.TrimStart().StartsWith('#') }) | Select-Object -First 1
if (-not $subject)
{
    exit 0
}

# git 自己生成的标题与 rebase 的 autosquash 前缀放行。
$exempt = '^(Merge |Revert "|fixup! |squash! |amend! )'
if ($subject -match $exempt)
{
    exit 0
}

$problems = [System.Collections.Generic.List[string]]::new()

# type 固定为 Conventional Commits 的集合；scope 用逗号或斜杠分隔，允许英文小写与数字。
$pattern = '^(build|chore|ci|docs|feat|fix|perf|refactor|revert|style|test)(\([a-z0-9]+([/,] ?[a-z0-9]+)*\))?: \S'
if ($subject -notmatch $pattern)
{
    $problems.Add('标题不符合 type(scope): subject，例如 fix(theme): 广播后未刷新托盘图标')
}

if ($subject.Length -gt 72)
{
    $problems.Add("标题有 $($subject.Length) 个字符，超过 72 字符上限")
}

if ($problems.Count -gt 0)
{
    Write-Host ''
    Write-Host "commit-msg: $subject" -ForegroundColor Yellow
    foreach ($problem in $problems)
    {
        Write-Host "  - $problem" -ForegroundColor Yellow
    }
    Write-Host '  规则见 docs/engineering/09-conventions.md 的“提交信息”一节' -ForegroundColor DarkGray
    exit 1
}

exit 0
