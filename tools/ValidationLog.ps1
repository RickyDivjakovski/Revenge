# Shared, explicit line capture. Does not depend on host transcript behavior.
function Write-RevengeLine([string]$Message) {
    Write-Host $Message
    if ($LogPath) { Add-Content -LiteralPath $LogPath -Value $Message -Encoding UTF8 -ErrorAction Stop }
}
function Invoke-RevengeChecked([string]$Exe, [string[]]$Arguments) {
    Write-RevengeLine ("RUN: $Exe $Arguments")
    $oldPreference = $ErrorActionPreference
    try {
        # Windows PowerShell 5.1 represents native stderr as ErrorRecords.
        $ErrorActionPreference = 'Continue'
        if (-not (Get-Command $Exe -ErrorAction SilentlyContinue)) { throw "Executable not found: $Exe" }
        $global:LASTEXITCODE = $null
        & $Exe @Arguments 2>&1 | ForEach-Object { Write-RevengeLine ([string]$_) }
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $oldPreference }
    if ($null -eq $code -or $code -ne 0) { Write-RevengeLine "FAIL: $Exe (exit $code)"; throw "Failed ($code): $Exe $Arguments" }
    Write-RevengeLine "PASS: $Exe (exit 0)"
}
