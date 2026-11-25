param([Parameter(Mandatory=$true)][string]$Fixture, [Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$LogPath = $Output
Set-Content -LiteralPath $LogPath -Value 'test start' -Encoding UTF8
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'tools\ValidationLog.ps1')
try {
    $failed = $false
    try { Invoke-RevengeChecked $Fixture @('--log-probe') } catch {
        if ($_.Exception.Message -notmatch 'Failed \(7\)') { throw }
        $failed = $true
    }
    if (-not $failed) { throw 'Expected nonzero exit status was lost' }
    $content = Get-Content -LiteralPath $LogPath -Raw
    if ($content -notmatch 'reference-stdout-marker' -or $content -notmatch 'reference-stderr-marker') { throw 'Native stdout or stderr missing' }
    Write-Host 'Validation logging: stdout, stderr and nonzero native exit status preserved.'
} finally { Remove-Item -LiteralPath $LogPath -Force -ErrorAction SilentlyContinue }
