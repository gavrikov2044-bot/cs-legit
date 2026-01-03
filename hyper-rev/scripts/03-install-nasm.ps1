# ============================================================
# STEP 3: Install NASM (Netwide Assembler)
# ============================================================
# Run as Administrator!
# NASM is required to compile hyper-reV's UEFI module

Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "  hyper-reV Setup - Install NASM            " -ForegroundColor Cyan
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as admin
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[ERROR] This script must be run as Administrator!" -ForegroundColor Red
    pause
    exit 1
}

# Check if NASM is already installed
$nasmPath = Get-Command nasm -ErrorAction SilentlyContinue
if ($nasmPath) {
    Write-Host "[OK] NASM is already installed: $($nasmPath.Source)" -ForegroundColor Green
    nasm -v
    
    # Check NASM_PREFIX
    $nasmPrefix = [Environment]::GetEnvironmentVariable("NASM_PREFIX", "Machine")
    if ($nasmPrefix) {
        Write-Host "[OK] NASM_PREFIX is set: $nasmPrefix" -ForegroundColor Green
    } else {
        Write-Host "[WARN] NASM_PREFIX not set, setting now..." -ForegroundColor Yellow
        $nasmDir = Split-Path $nasmPath.Source
        [Environment]::SetEnvironmentVariable("NASM_PREFIX", "$nasmDir\", "Machine")
        Write-Host "[OK] NASM_PREFIX set to: $nasmDir\" -ForegroundColor Green
    }
    
    Write-Host ""
    Write-Host "NASM is ready! Press any key to continue to step 04..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit 0
}

# NASM not found, need to install
Write-Host "[INFO] NASM not found. Installing..." -ForegroundColor Yellow
Write-Host ""

# Download NASM
$nasmVersion = "2.16.01"
$nasmUrl = "https://www.nasm.us/pub/nasm/releasebuilds/$nasmVersion/win64/nasm-$nasmVersion-win64.zip"
$downloadPath = "$env:TEMP\nasm-$nasmVersion-win64.zip"
$extractPath = "C:\NASM"

Write-Host "[STEP 1/4] Downloading NASM $nasmVersion..." -ForegroundColor Yellow
try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $nasmUrl -OutFile $downloadPath -UseBasicParsing
    Write-Host "[OK] Downloaded to $downloadPath" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Failed to download NASM: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please download manually from: https://www.nasm.us/" -ForegroundColor Yellow
    pause
    exit 1
}

# Extract
Write-Host ""
Write-Host "[STEP 2/4] Extracting NASM..." -ForegroundColor Yellow
try {
    if (Test-Path $extractPath) {
        Remove-Item $extractPath -Recurse -Force
    }
    Expand-Archive -Path $downloadPath -DestinationPath "C:\" -Force
    Rename-Item "C:\nasm-$nasmVersion" $extractPath
    Write-Host "[OK] Extracted to $extractPath" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Failed to extract: $_" -ForegroundColor Red
    pause
    exit 1
}

# Add to PATH
Write-Host ""
Write-Host "[STEP 3/4] Adding NASM to PATH..." -ForegroundColor Yellow
$currentPath = [Environment]::GetEnvironmentVariable("PATH", "Machine")
if ($currentPath -notlike "*$extractPath*") {
    [Environment]::SetEnvironmentVariable("PATH", "$currentPath;$extractPath", "Machine")
    Write-Host "[OK] Added to system PATH" -ForegroundColor Green
} else {
    Write-Host "[OK] Already in PATH" -ForegroundColor Green
}

# Set NASM_PREFIX (required by EDK2)
Write-Host ""
Write-Host "[STEP 4/4] Setting NASM_PREFIX environment variable..." -ForegroundColor Yellow
[Environment]::SetEnvironmentVariable("NASM_PREFIX", "$extractPath\", "Machine")
Write-Host "[OK] NASM_PREFIX set to: $extractPath\" -ForegroundColor Green

# Update current session
$env:PATH = "$env:PATH;$extractPath"
$env:NASM_PREFIX = "$extractPath\"

# Verify
Write-Host ""
Write-Host "[INFO] Verifying installation..." -ForegroundColor Yellow
& "$extractPath\nasm.exe" -v

Write-Host ""
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host "  NASM installed successfully!              " -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "[IMPORTANT] Close and reopen any terminals to apply PATH changes!" -ForegroundColor Yellow
Write-Host ""
Write-Host "Press any key to continue to step 04..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")

