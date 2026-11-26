param([switch]$Play, [switch]$Hardware, [string]$AuthorisedTarget = '', [string]$ProbeTrace = '', [string[]]$DependencyDlls = @(), [ValidateRange(1,64)][int]$BuildJobs = 1, [switch]$FastBuild, [switch]$Focused, [string]$GdkRoot = '', [switch]$SkipGdk)
$ErrorActionPreference = 'Stop'
if ($Focused -and ($Play -or $Hardware -or $ProbeTrace -or $DependencyDlls.Count -gt 0)) { throw 'Focused mode cannot be combined with gameplay, hardware or external target checks. Use full validation.' }
Set-Location $PSScriptRoot
# The log lives in a fresh directory, so prior reports and save files are never overwritten.
$run = Join-Path $PSScriptRoot ('validation-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + $PID)
New-Item -ItemType Directory -Path $run | Out-Null
$LogPath = Join-Path $run 'validation.log'
Set-Content -LiteralPath $LogPath -Value ('Revenge validation ' + (Get-Date -Format o)) -Encoding UTF8
. (Join-Path $PSScriptRoot 'tools\ValidationLog.ps1')
try {
    if ($Focused) { Write-RevengeLine '=== Build and focused graphics regression suite ===' } else { Write-RevengeLine '=== Build and full regression suite ===' }
    & (Join-Path $PSScriptRoot 'Build-Revenge.ps1') -BuildJobs $BuildJobs -LogPath $LogPath -FastBuild:$FastBuild -Focused:$Focused -GdkRoot $GdkRoot -SkipGdk:$SkipGdk -GpuTests
    $bin = Join-Path $PSScriptRoot 'build\Release'
    $rev = Join-Path $bin 'Revenge.exe'
    Invoke-RevengeChecked $rev @('--version')
    $arena = Join-Path $bin 'RevengeArena.exe'
    $abi = Join-Path $bin 'revenge_abi.dll'
    $probe = Join-Path $bin 'RevengePlatformProbe.exe'
    $commands = Join-Path $bin 'revenge_abi_commands_fixture.exe'
    Write-RevengeLine '=== Required desktop GPU regression group (seven tests) ==='
    Invoke-RevengeChecked ctest @('--test-dir', 'build', '-C', 'Release', '--verbose', '-L', '^desktop_gpu$', '--no-tests=error')
    Write-RevengeLine '=== Public GDK API inventory and independent manual queue ==='
    Invoke-RevengeChecked (Join-Path $bin 'revenge_manual_queue_tests.exe') @()
    Invoke-RevengeChecked (Join-Path $bin 'revenge_scheduled_queue_tests.exe') @()
    Invoke-RevengeChecked $rev @('gdk-api-registry')
    $gdkEnabled = (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'build\revenge-gdk-enabled.txt') -Raw).Trim()
    if ($gdkEnabled -eq 'ON') {
        $gdkRef = Join-Path $bin 'RevengeGdkReference.exe'
        if (-not $Focused) { Invoke-RevengeChecked $gdkRef @() }
        Write-RevengeLine '=== Public GDK reference executable offline diagnostics ==='
        Invoke-RevengeChecked $rev @('compatibility-plan', $gdkRef)
        Invoke-RevengeChecked $rev @('gdk-import-plan', $gdkRef)
        Invoke-RevengeChecked $rev @('import-plan', $gdkRef)
    } else { Write-RevengeLine 'SKIP: Public GDK reference disabled or not detected; no SDK runtime validation.' }
    if ($Focused) {
        if ($AuthorisedTarget) {
            $target = (Resolve-Path -LiteralPath $AuthorisedTarget).Path
            Invoke-RevengeChecked $rev @('compatibility-plan', $target)
        }
        Write-RevengeLine '=== PASS: Focused validation complete; SKIP: full regression not run ==='
        return
    }
    $color = Join-Path $run 'color.rsp'
    $legacy = Join-Path $run 'legacy.rsp'
    $narrow = Join-Path $run 'narrow.rsp'
    $minmax = Join-Path $run 'minmax.rsp'
    Invoke-RevengeChecked $rev @('package-create-color', $color)
    Invoke-RevengeChecked $rev @('package-create', $legacy)
    Invoke-RevengeChecked $rev @('package-create-narrow', $narrow)
    Invoke-RevengeChecked $rev @('package-create-minmax', $minmax)
    Write-RevengeLine '=== Integrated GPU gameplay and shader comparison ==='
    $mode = if ($Hardware) { '--hardware' } else { '--gpu' }
    Invoke-RevengeChecked $arena @('--selftest', $color, $legacy, $mode)
    Write-RevengeLine '=== Custom vertex shader GPU validation ==='
    Invoke-RevengeChecked $commands @($narrow, '--vertex-gpu')
    Write-RevengeLine '=== Min/max vertex GPU comparison ==='
    Invoke-RevengeChecked $commands @($minmax, '--vertex-gpu')
    Write-RevengeLine '=== Extended CPU texture reference ==='
    Invoke-RevengeChecked (Join-Path $bin 'revenge_texture_sampling_tests.exe') @()
    Write-RevengeLine '=== Shared game workload and native host ABI ==='
    Invoke-RevengeChecked $probe @()
    if ($ProbeTrace) { Invoke-RevengeChecked $probe @('--compare', (Resolve-Path -LiteralPath $ProbeTrace).Path) }
    Write-RevengeLine 'GDKX source probe is not built or run by this script; no Xbox execution.'
    Write-RevengeLine '=== Prior GPU stress regression ==='
    Invoke-RevengeChecked $commands @($legacy, '--async-gpu-stress')
    Write-RevengeLine '=== Fixed self-authored native image execution fixture ==='
    Invoke-RevengeChecked (Join-Path $bin 'revenge_native_image_fixture.exe') @()
    Write-RevengeLine '=== Compiler-built reference DLL lifecycle ==='
    Invoke-RevengeChecked (Join-Path $bin 'revenge_reference_fixture.exe') @()
    $reference = Join-Path $bin 'revenge_reference.dll'
    Invoke-RevengeChecked $rev @('exports', $reference)
    Invoke-RevengeChecked $rev @('init-plan', $reference)
    Invoke-RevengeChecked $rev @('tls-plan', $reference)
    Invoke-RevengeChecked (Join-Path $bin 'revenge_tls_template_tests.exe') @()
    Invoke-RevengeChecked (Join-Path $bin 'revenge_stage_constants_tests.exe') @()
    Invoke-RevengeChecked $rev @('dependency-chain', (Join-Path $bin 'revenge_reference_fixture.exe'), $reference, $abi)
    Invoke-RevengeChecked $rev @('dependency-graph', (Join-Path $bin 'revenge_reference_fixture.exe'), $reference, $abi)
    Write-RevengeLine '=== Native application initialisation and export diagnostics ==='
    Invoke-RevengeChecked $rev @('target-plan', $probe)
    Invoke-RevengeChecked $rev @('init-plan', $arena)
    Invoke-RevengeChecked $rev @('dependency-chain', $arena, $abi)
    if ($AuthorisedTarget) {
        $target = (Resolve-Path -LiteralPath $AuthorisedTarget).Path
        Write-RevengeLine '=== Authorised target: offline reports only ==='
        Invoke-RevengeChecked $rev @('compatibility-plan', $target)
        Invoke-RevengeChecked $rev @('inspect', $target)
        Invoke-RevengeChecked $rev @('layout-plan', $target)
        Invoke-RevengeChecked $rev @('init-plan', $target)
        Invoke-RevengeChecked $rev @('import-plan', $target)
        if ($DependencyDlls.Count -gt 0) {
            $resolved = @($DependencyDlls | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
            Invoke-RevengeChecked $rev (@('dependency-chain', $target) + $resolved)
            Invoke-RevengeChecked $rev (@('dependency-graph', $target) + $resolved)
        }
    }
    Write-RevengeLine '=== Automated validation complete ==='
    if (-not $Hardware) { Write-RevengeLine 'Audio and physical controller checks not requested; the interactive game can exercise them.' }
    if ($Play) {
        Write-RevengeLine '=== Revenge Arena === WASD/arrows/stick; collect four gold blocks; F5 save, F9 load, R reset, Esc quit.'
        Invoke-RevengeChecked $arena @($color, (Join-Path $PSScriptRoot 'arena-save.rvs'))
    }
} catch {
    Write-RevengeLine ("VALIDATION FAILED: " + $_.Exception.Message)
    throw
} finally {
    Write-RevengeLine "Validation log: $LogPath"
}
