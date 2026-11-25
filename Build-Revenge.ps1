$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
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
cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
Write-Host 'Executable: build\Release\Revenge.exe'
