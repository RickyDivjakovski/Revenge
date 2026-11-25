param([string]$AgilityDirectory = '', [uint32]$AgilityVersion = 0, [ValidateRange(1,64)][int]$BuildJobs = 1, [string]$LogPath = '', [switch]$FastBuild, [switch]$Focused)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
. (Join-Path $PSScriptRoot 'tools\ValidationLog.ps1')
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    $locations = @("$env:ProgramFiles\CMake\bin\cmake.exe")
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) { $locations += "$vsPath\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" }
    }
    foreach ($location in $locations) {
        if (Test-Path $location) { $env:Path = "$(Split-Path $location);$env:Path"; break }
    }
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { throw 'CMake not found. Install Kitware.CMake or the Visual Studio C++ CMake component, then reopen PowerShell.' }
if (($AgilityDirectory -eq '') -ne ($AgilityVersion -eq 0)) { throw 'Supply both -AgilityDirectory and -AgilityVersion, or neither.' }
$ipo = if ($FastBuild) { 'OFF' } else { 'ON' }
Write-RevengeLine "Build configuration: Release; IPO=$ipo; jobs=$BuildJobs"
if ($AgilityDirectory) {
    $AgilityDirectory = (Resolve-Path -LiteralPath $AgilityDirectory).Path
    Invoke-RevengeChecked cmake @('-S', '.', '-B', 'build', '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DREVENGE_AGILITY_DIR=$AgilityDirectory", "-DREVENGE_AGILITY_VERSION=$AgilityVersion", "-DREVENGE_ENABLE_IPO=$ipo")
} else {
    Invoke-RevengeChecked cmake @('-S', '.', '-B', 'build', '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DREVENGE_AGILITY_DIR=', '-DREVENGE_AGILITY_VERSION=', "-DREVENGE_ENABLE_IPO=$ipo")
}
$buildTimer = [System.Diagnostics.Stopwatch]::StartNew()
if ($Focused) {
    Invoke-RevengeChecked cmake @('--build', 'build', '--config', 'Release', '--parallel', "$BuildJobs", '--target', 'Revenge', 'revenge_texture_tests', 'revenge_texture_sampling_tests', 'revenge_sampling_workload_tests', 'revenge_render_pass_tests')
} else {
    Invoke-RevengeChecked cmake @('--build', 'build', '--config', 'Release', '--parallel', "$BuildJobs")
}
$buildTimer.Stop()
Write-RevengeLine ('Build elapsed seconds: ' + $buildTimer.Elapsed.TotalSeconds.ToString('F2'))
if ($Focused) {
    Write-RevengeLine 'Focused regression: texture and render-pass CPU references only.'
    Invoke-RevengeChecked ctest @('--test-dir', 'build', '-C', 'Release', '--output-on-failure', '-R', '^(texture_(core|sampling_extended|gpu_workload)|render_pass_reference)$', '--no-tests=error')
} else {
    Invoke-RevengeChecked ctest @('--test-dir', 'build', '-C', 'Release', '--output-on-failure')
}
Write-RevengeLine 'Executable: build\Release\Revenge.exe'
