#Requires -Version 7.0
<#
.SYNOPSIS
启用或停用仓库自带的 Git 钩子（.githooks）。

.DESCRIPTION
Git 只认 .git/hooks 下的钩子，那个目录不进版本库。这里把 core.hooksPath 指到
.githooks，克隆后执行一次即可。钩子只是本地快速反馈，CI 才是门禁。
#>
param(
    [switch]$Uninstall,
    [switch]$Status
)

$repoRoot = Split-Path -Parent $PSScriptRoot
Push-Location $repoRoot
try
{
    if ($Status)
    {
        $current = (& git config --local --get core.hooksPath)
        if ($LASTEXITCODE -ne 0)
        {
            Write-Host 'core.hooksPath 未设置，钩子未启用'
        }
        else
        {
            Write-Host "core.hooksPath = $current"
        }
        exit 0
    }

    if ($Uninstall)
    {
        & git config --local --unset core.hooksPath
        if ($LASTEXITCODE -ne 0)
        {
            Write-Host 'core.hooksPath 本来就没设置'
        }
        else
        {
            Write-Host '已停用本地钩子'
        }
        exit 0
    }

    & git config --local core.hooksPath .githooks
    if ($LASTEXITCODE -ne 0)
    {
        throw '写入 core.hooksPath 失败'
    }
    Write-Host '已启用 .githooks：提交时会先跑本地检查，git commit --no-verify 可跳过'
}
finally
{
    Pop-Location
}
