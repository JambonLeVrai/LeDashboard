<#
.SYNOPSIS
    Installs the com0com virtual null-modem driver and creates a COM port pair
    (default COM20 <-> COM21) for testing the LeDashboard server without the
    ESP32 hardware attached.

.NOTES
    MUST be run from an elevated PowerShell (Run as Administrator).

    com0com is a TEST-SIGNED kernel driver: on x64 Windows it only loads when
    test signing is enabled. If it is off, re-run with -EnableTestSigning
    (or run `bcdedit -set testsigning ON` yourself) and REBOOT before using
    the ports. Enabling test signing lowers OS driver-signing security.

    After setup: point the server at one port (e.g. COM20) and run
    server\tools\mock_esp32.py on the other (e.g. COM21).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File server\tools\setup_com0com.ps1 -EnableTestSigning
#>
#Requires -RunAsAdministrator

param(
    [string]$Com0ComDir = "C:\Program Files (x86)\com0com",
    [string]$PortA = "COM20",
    [string]$PortB = "COM21",
    [switch]$EnableTestSigning
)

$ErrorActionPreference = "Stop"
$setupc = Join-Path $Com0ComDir "setupc.exe"

if (-not (Test-Path $setupc)) {
    Write-Error "setupc.exe not found at '$setupc'. Install com0com first (choco install com0com)."
    exit 1
}

# 1. Test signing (required for the test-signed com0com.sys on x64)
$bcd = bcdedit /enum "{current}" 2>$null
if ($bcd -match "testsigning\s+Yes") {
    Write-Host "Test signing: ON"
} else {
    if ($EnableTestSigning) {
        bcdedit -set testsigning on | Out-Null
        Write-Host "Test signing enabled. REBOOT is required before the driver will load."
        Write-Host "After reboot, re-run this script WITHOUT -EnableTestSigning to create the port pair."
        exit 0
    }
    Write-Warning "Test signing is OFF - the com0com driver will NOT load. Re-run with -EnableTestSigning, then reboot."
    exit 1
}

# 2. Install the driver if no com0com device exists yet
$existing = Get-PnpDevice -Class Ports -ErrorAction SilentlyContinue | Where-Object { $_.FriendlyName -like "*com0com*" -or $_.FriendlyName -like "CNC*" }
if (-not $existing) {
    Write-Host "Installing com0com driver..."
    & $setupc install com0com.inf com0com
    if ($LASTEXITCODE -ne 0) { Write-Error "driver install failed (exit $LASTEXITCODE)"; exit 1 }
} else {
    Write-Host "com0com driver already present."
}

# 3. Create a new virtual port pair (default names CNCAx / CNCBx)
Write-Host "Creating virtual port pair..."
& $setupc --no-update install - -
& $setupc update
if ($LASTEXITCODE -ne 0) { Write-Error "pair creation failed (exit $LASTEXITCODE)"; exit 1 }

# 4. Rename the newest pair to $PortA / $PortB
$ports = Get-PnpDevice -Class Ports -ErrorAction SilentlyContinue |
    Where-Object { $_.FriendlyName -match "CNCA(\d+)" -or $_.FriendlyName -match "CNCB(\d+)" } |
    ForEach-Object { $_.FriendlyName }
$ca = ($ports | Where-Object { $_ -match "^CNCA\d+$" } | Sort-Object { [int]($_ -replace '\D','') } | Select-Object -Last 1)
$cb = ($ports | Where-Object { $_ -match "^CNCB\d+$" } | Sort-Object { [int]($_ -replace '\D','') } | Select-Object -Last 1)
if (-not $ca -or -not $cb) {
    Write-Warning "No CNCAx/CNCBx ports found after install. List ports with: setupc list"
    exit 1
}
Write-Host "Renaming $ca -> $PortA and $cb -> $PortB"
& $setupc change $ca "PortName=$PortA"
& $setupc change $cb "PortName=$PortB"

# 5. Verify
Write-Host ""
Write-Host "Available COM ports:"
[System.IO.Ports.SerialPort]::GetPortNames() -join ", " | Write-Host
Write-Host ""
Write-Host "Done. Use $PortA for the server and $PortB for mock_esp32.py (or vice versa)."
