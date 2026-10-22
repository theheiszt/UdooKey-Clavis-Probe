# Flash helper script for UDOO KEY RP2040
$uf2Path = Join-Path $PSScriptRoot "rp2040_logic_analyzer\.pio\build\rp2040_logic_analyzer\firmware.uf2"

if (-not (Test-Path $uf2Path)) {
    Write-Host "[!] firmware.uf2 not found. Compiling first..." -ForegroundColor Yellow
    & "C:\Users\Andrew\.platformio\penv\Scripts\pio.exe" run -d "rp2040_logic_analyzer"
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   UDOO KEY RP2040 One-Click UF2 Flasher                  " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Source: $uf2Path" -ForegroundColor Gray
Write-Host ""
Write-Host "Looking for RPI-RP2 drive..." -ForegroundColor Yellow

$found = $false
$rp2Drive = Get-Volume | Where-Object { $_.FileSystemLabel -eq "RPI-RP2" }

if ($rp2Drive) {
    $targetPath = "$($rp2Drive.DriveLetter):\"
    Write-Host "[+] Found RPI-RP2 drive at $targetPath!" -ForegroundColor Green
    Copy-Item $uf2Path -Destination $targetPath -Force
    Write-Host "[SUCCESS] firmware.uf2 successfully copied to $targetPath! Board is rebooting..." -ForegroundColor Green
    $found = $true
} else {
    Write-Host "[-] RPI-RP2 drive not currently mounted." -ForegroundColor Red
    Write-Host ""
    Write-Host "To enter BOOTSEL mode on the UDOO KEY:" -ForegroundColor Yellow
    Write-Host "  1. Ensure JP1 is CLOSED (routes USB-C to RP2040)." -ForegroundColor White
    Write-Host "  2. Press and HOLD SW2 (BOOTSEL, at bottom of board)." -ForegroundColor White
    Write-Host "  3. While holding SW2, press and release SW1 (RP2040 Reset)." -ForegroundColor White
    Write-Host "  4. Release SW2." -ForegroundColor White
    Write-Host "  5. The board will mount as 'RPI-RP2' drive in Windows." -ForegroundColor White
    Write-Host ""
    Write-Host "Waiting up to 30 seconds for RPI-RP2 to mount..." -ForegroundColor Cyan

    for ($i = 0; $i -lt 30; $i++) {
        Start-Sleep -Seconds 1
        $rp2Drive = Get-Volume | Where-Object { $_.FileSystemLabel -eq "RPI-RP2" }
        if ($rp2Drive) {
            $targetPath = "$($rp2Drive.DriveLetter):\"
            Write-Host "`n[+] Found RPI-RP2 drive at $targetPath!" -ForegroundColor Green
            Copy-Item $uf2Path -Destination $targetPath -Force
            Write-Host "[SUCCESS] firmware.uf2 successfully copied to $targetPath! Board is rebooting..." -ForegroundColor Green
            $found = $true
            break
        }
        Write-Host -NoNewline "."
    }

    if (-not $found) {
        Write-Host "`n[!] Timed out waiting for RPI-RP2. Run this script again after pressing SW2+SW1." -ForegroundColor Yellow
    }
}
