# ============================================================
# STEP 1: Create Windows System Restore Point
# ============================================================
# Run as Administrator!
# This creates a restore point BEFORE we modify anything

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "  hyper-reV Safety Script - Restore Point   " -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as admin
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[ERROR] This script must be run as Administrator!" -ForegroundColor Red
    Write-Host "Right-click PowerShell -> Run as Administrator" -ForegroundColor Yellow
    pause
    exit 1
}

Write-Host "[INFO] Running as Administrator - OK" -ForegroundColor Green
Write-Host ""

# Enable System Restore if disabled
Write-Host "[STEP 1/3] Enabling System Restore on C: drive..." -ForegroundColor Yellow
try {
    Enable-ComputerRestore -Drive "C:\" -ErrorAction SilentlyContinue
    Write-Host "[OK] System Restore enabled" -ForegroundColor Green
} catch {
    Write-Host "[WARN] Could not enable System Restore: $_" -ForegroundColor Yellow
}

# Create restore point
Write-Host ""
Write-Host "[STEP 2/3] Creating restore point 'Before hyper-reV'..." -ForegroundColor Yellow
try {
    Checkpoint-Computer -Description "Before hyper-reV Installation" -RestorePointType "MODIFY_SETTINGS" -ErrorAction Stop
    Write-Host "[OK] Restore point created successfully!" -ForegroundColor Green
} catch {
    # Sometimes fails if restore point was created recently (within 24h)
    Write-Host "[WARN] Could not create restore point: $_" -ForegroundColor Yellow
    Write-Host "[INFO] This usually means a restore point was created recently" -ForegroundColor Cyan
}

# Verify
Write-Host ""
Write-Host "[STEP 3/3] Verifying restore points..." -ForegroundColor Yellow
$restorePoints = Get-ComputerRestorePoint | Select-Object -Last 3
if ($restorePoints) {
    Write-Host "[OK] Recent restore points:" -ForegroundColor Green
    $restorePoints | Format-Table -Property SequenceNumber, Description, CreationTime -AutoSize
} else {
    Write-Host "[WARN] No restore points found" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "  Restore point created!                     " -ForegroundColor Green
Write-Host "  You can now proceed to step 02             " -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Press any key to continue..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

