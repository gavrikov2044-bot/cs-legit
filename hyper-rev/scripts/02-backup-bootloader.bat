@echo off
REM ============================================================
REM STEP 2: Backup Windows Bootloader (bootmgfw.efi)
REM ============================================================
REM Run as Administrator!
REM This creates a backup of the original bootloader

echo =============================================
echo   hyper-reV Safety Script - Backup Bootloader
echo =============================================
echo.

REM Check for admin rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] This script must be run as Administrator!
    echo Right-click CMD -^> Run as Administrator
    pause
    exit /b 1
)

echo [INFO] Running as Administrator - OK
echo.

REM Create backup directory
set BACKUP_DIR=%~dp0..\backup
if not exist "%BACKUP_DIR%" (
    mkdir "%BACKUP_DIR%"
    echo [OK] Created backup directory: %BACKUP_DIR%
)

REM Mount EFI partition
echo [STEP 1/4] Mounting EFI partition...
mountvol X: /s
if %errorlevel% neq 0 (
    echo [ERROR] Failed to mount EFI partition!
    echo Trying alternative method...
    
    REM Try diskpart method
    echo select disk 0 > %TEMP%\mount_efi.txt
    echo list vol >> %TEMP%\mount_efi.txt
    diskpart /s %TEMP%\mount_efi.txt
    
    echo.
    echo [INFO] Please mount EFI partition manually using diskpart
    echo       Then re-run this script
    pause
    exit /b 1
)
echo [OK] EFI partition mounted on X:
echo.

REM Check if bootmgfw.efi exists
echo [STEP 2/4] Locating bootmgfw.efi...
if not exist "X:\EFI\Microsoft\Boot\bootmgfw.efi" (
    echo [ERROR] bootmgfw.efi not found at expected location!
    echo Looking for it...
    dir /s /b X:\bootmgfw.efi 2>nul
    mountvol X: /d
    pause
    exit /b 1
)
echo [OK] Found: X:\EFI\Microsoft\Boot\bootmgfw.efi
echo.

REM Get file info
echo [STEP 3/4] Getting file information...
for %%F in ("X:\EFI\Microsoft\Boot\bootmgfw.efi") do (
    echo     Size: %%~zF bytes
    echo     Date: %%~tF
)
echo.

REM Create backup with timestamp
set TIMESTAMP=%date:~-4%%date:~3,2%%date:~0,2%_%time:~0,2%%time:~3,2%
set TIMESTAMP=%TIMESTAMP: =0%
set BACKUP_FILE=%BACKUP_DIR%\bootmgfw_original_%TIMESTAMP%.efi

echo [STEP 4/4] Creating backup...
copy "X:\EFI\Microsoft\Boot\bootmgfw.efi" "%BACKUP_FILE%" >nul
if %errorlevel% neq 0 (
    echo [ERROR] Failed to create backup!
    mountvol X: /d
    pause
    exit /b 1
)
echo [OK] Backup created: %BACKUP_FILE%

REM Also create a "latest" copy for easy recovery
copy "X:\EFI\Microsoft\Boot\bootmgfw.efi" "%BACKUP_DIR%\bootmgfw_original.efi" >nul
echo [OK] Latest backup: %BACKUP_DIR%\bootmgfw_original.efi

REM Unmount EFI partition
echo.
echo [INFO] Unmounting EFI partition...
mountvol X: /d
echo [OK] EFI partition unmounted
echo.

echo =============================================
echo   Bootloader backup complete!
echo =============================================
echo.
echo Backup location: %BACKUP_DIR%
echo.
echo [IMPORTANT] Keep this backup safe!
echo If hyper-reV causes boot issues:
echo 1. Boot from Windows USB
echo 2. Copy bootmgfw_original.efi back to EFI partition
echo 3. Or run recovery-hyper-rev.bat
echo.
echo Press any key to continue to step 03...
pause >nul

