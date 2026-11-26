param([string]$AgilityDirectory = '', [uint32]$AgilityVersion = 0, [ValidateRange(1,64)][int]$BuildJobs = 1, [string]$LogPath = '', [switch]$FastBuild, [switch]$Focused, [string]$GdkRoot = '', [switch]$SkipGdk, [switch]$GpuTests)
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
$gdk = if ($SkipGdk) { 'OFF' } else { 'ON' }
if ($GdkRoot) { $GdkRoot = (Resolve-Path -LiteralPath $GdkRoot).Path }
$gpuTestsFlag = if ($GpuTests) { 'ON' } else { 'OFF' }
$ipo = if ($FastBuild) { 'OFF' } else { 'ON' }
Write-RevengeLine "Build configuration: Release; IPO=$ipo; jobs=$BuildJobs"
if ($AgilityDirectory) {
    $AgilityDirectory = (Resolve-Path -LiteralPath $AgilityDirectory).Path
    Invoke-RevengeChecked cmake @('-S', '.', '-B', 'build', '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DREVENGE_AGILITY_DIR=$AgilityDirectory", "-DREVENGE_AGILITY_VERSION=$AgilityVersion", "-DREVENGE_ENABLE_IPO=$ipo", "-DREVENGE_ENABLE_GPU_TESTS=$gpuTestsFlag", "-DREVENGE_ENABLE_GDK=$gdk", "-DREVENGE_GDK_ROOT=$GdkRoot")
} else {
    Invoke-RevengeChecked cmake @('-S', '.', '-B', 'build', '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DREVENGE_AGILITY_DIR=', '-DREVENGE_AGILITY_VERSION=', "-DREVENGE_ENABLE_IPO=$ipo", "-DREVENGE_ENABLE_GPU_TESTS=$gpuTestsFlag", "-DREVENGE_ENABLE_GDK=$gdk", "-DREVENGE_GDK_ROOT=$GdkRoot")
}
$buildTimer = [System.Diagnostics.Stopwatch]::StartNew()
if ($Focused) {
    Invoke-RevengeChecked cmake @('--build', 'build', '--config', 'Release', '--parallel', "$BuildJobs", '--target', 'revenge_focused')
} else {
    Invoke-RevengeChecked cmake @('--build', 'build', '--config', 'Release', '--parallel', "$BuildJobs")
}
$buildTimer.Stop()
Write-RevengeLine ('Build elapsed seconds: ' + $buildTimer.Elapsed.TotalSeconds.ToString('F2'))
if ($Focused) {
    Write-RevengeLine 'Focused regression: graphics CPU references, manual, scheduled and native C task queues, SDK detection, API inventory and optional Windows GDK reference.'
    Invoke-RevengeChecked ctest @('--test-dir', 'build', '-C', 'Release', '--verbose', '-R', '^(texture_(core|sampling_extended|gpu_workload)|render_pass_reference|native_abi_multipass|manual_queue_reference|scheduled_queue_reference|gdk_detection|gdk_api_inventory|public_gdk_reference|queue_c_abi|queue_native_integration|queue_tracked_cancellation|queue_retained_operations|queue_operation_races|async_application|gpu_validation_contract|native_fixed_image|native_resource_transfer|resident_resources_contract|resident_draw_contract|executable_compatibility_contract)$', '--no-tests=error')
} else {
    Invoke-RevengeChecked ctest @('--test-dir', 'build', '-C', 'Release', '--output-on-failure', '-LE', 'desktop_gpu')
}
Write-RevengeLine 'Executable: build\Release\Revenge.exe'
