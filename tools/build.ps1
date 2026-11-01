[CmdletBinding()]
param(
    [ValidateSet('x64-debug', 'x64-release', 'arm64-release')]
    [string]$Preset = 'x64-release',

    [ValidateSet('ninja', 'nmake', 'vs2022')]
    [string]$Generator = 'ninja',

    [switch]$Test,

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build\$Preset-$Generator"

if ($Clean -and (Test-Path -LiteralPath $buildDir)) {
    $resolved = (Resolve-Path -LiteralPath $buildDir).Path
    $expectedRoot = (Join-Path $root 'build') + [IO.Path]::DirectorySeparatorChar
    if (-not $resolved.StartsWith($expectedRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "拒绝删除非 build 目录：$resolved"
    }
    Remove-Item -LiteralPath $resolved -Recurse -Force
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "找不到 vswhere.exe：$vswhere"
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
    Select-Object -First 1
if (-not $vsPath) {
    throw '找不到安装 C++ 工具集的 Visual Studio。'
}

$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "找不到 vcvarsall.bat：$vcvars"
}

$arch = if ($Preset.StartsWith('arm64', [StringComparison]::OrdinalIgnoreCase)) { 'arm64' } else { 'x64' }
$configuration = if ($Preset.EndsWith('debug', [StringComparison]::OrdinalIgnoreCase)) { 'Debug' } else { 'Release' }

# 预设只描述 Ninja；NMake / Visual Studio 用于无法使用 Ninja 的环境（如受限沙箱）。
if ($Generator -eq 'ninja') {
    $configure = "cmake --preset $Preset"
    $build = "cmake --build --preset $Preset"
}
elseif ($Generator -eq 'vs2022') {
    $cmakeArch = if ($arch -eq 'arm64') { 'ARM64' } else { 'x64' }
    $generatorName = 'Visual Studio 17 2022'
    $configure = "cmake -S `"$root`" -B `"$buildDir`" -G `"$generatorName`" -A $cmakeArch"
    $build = "cmake --build `"$buildDir`" --config $configuration"
}
else {
    $generatorName = 'NMake Makefiles'
    $configure = "cmake -S `"$root`" -B `"$buildDir`" -G `"$generatorName`" -DCMAKE_BUILD_TYPE=$configuration"
    $build = "cmake --build `"$buildDir`""
}

$commands = @(
    "call `"$vcvars`" $arch",
    $configure,
    $build
)

if ($Test) {
    if ($arch -ne 'x64') {
        Write-Warning 'ARM64 测试不在本机执行；跳过 ctest。'
    }
    elseif ($Generator -eq 'vs2022') {
        $commands += "ctest --test-dir `"$buildDir`" -C $configuration --output-on-failure"
    }
    else {
        $commands += "ctest --test-dir `"$buildDir`" --output-on-failure"
    }
}

$commandLine = $commands -join ' && '
Write-Host "> $commandLine"
& cmd.exe /c $commandLine
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
