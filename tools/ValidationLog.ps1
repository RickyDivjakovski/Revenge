# Shared, explicit line capture. Does not depend on host transcript behavior.
function Write-RevengeLine([string]$Message) {
    Write-Host $Message
    if ($LogPath) { Add-Content -LiteralPath $LogPath -Value $Message -Encoding UTF8 -ErrorAction Stop }
}
function Invoke-RevengeChecked([string]$Exe, [string[]]$Arguments) {
    $oldPreference = $ErrorActionPreference
    try {
        # Windows PowerShell 5.1 represents native stderr as ErrorRecords.
        $ErrorActionPreference = 'Continue'
        if (-not (Get-Command $Exe -ErrorAction SilentlyContinue)) { throw "Executable not found: $Exe" }
        & $Exe @Arguments 2>&1 | ForEach-Object { Write-RevengeLine ([string]$_) }
        $code = $LASTEXITCODE
    } finally { $ErrorActionPreference = $oldPreference }
    if ($code -ne 0) { throw "Failed ($code): $Exe $Arguments" }
}
