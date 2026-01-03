@echo off
REM ============================================================
REM STEP 7: Deploy hyper-reV to EFI partition
REM ============================================================
REM ⚠️ WARNING: This modifies your bootloader!
REM Make sure you have backups before proceeding!

echo =============================================
echo   hyper-reV Setup - DEPLOY
echo =============================================
echo.
echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
echo !  WARNING: This will modify your bootloader!
echo !  Make sure you have:
echo !  1. A restore point (step 01)
echo !  2. Backup of bootmgfw.efi (step 02)
echo !  3. Windows USB for recovery
echo !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
echo.

REM Check for admin rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] This script must be run as Administrator!
    pause
    exit /b 1
)

REM Set paths
set SCRIPT_DIR=%~dp0
set BUILD_DIR=%SCRIPT_DIR%..\build
set BACKUP_DIR=%SCRIPT_DIR%..\backup

REM Check if build files exist
if not exist "%BUILD_DIR%\uefi-boot.efi" (
    echo [ERROR] uefi-boot.efi not found!
    echo Please run 06-build-hyper-rev.bat first
    pause
    exit /b 1
)
if not exist "%BUILD_DIR%\hyperv-attachment.dll" (
    echo [ERROR] hyperv-attachment.dll not found!
    echo Please run 06-build-hyper-rev.bat first
    pause
    exit /b 1
)

REM Check if backup exists
if not exist "%BACKUP_DIR%\bootmgfw_original.efi" (
    echo [ERROR] Bootloader backup not found!
    echo Please run 02-backup-bootloader.bat first
    pause
    exit /b 1
)

echo [OK] All prerequisites met
echo.
echo Build files:
echo   - uefi-boot.efi
echo   - hyperv-attachment.dll
echo.
echo Backup:
echo   - bootmgfw_original.efi
echo.

REM Final confirmation
echo =============================================
echo   FINAL CONFIRMATION
echo =============================================
echo.
echo This will:
echo 1. Replace bootmgfw.efi with uefi-boot.efi
echo 2. Copy hyperv-attachment.dll to EFI partition
echo.
echo After reboot, hyper-reV will load BEFORE Windows.
echo.
set /p CONFIRM="Type 'DEPLOY' to continue, or anything else to cancel: "
if /i not "%CONFIRM%"=="DEPLOY" (
    echo.
    echo Deployment cancelled.
    pause
    exit /b 0
)

echo.
echo [DEPLOYING...]
echo.

REM Mount EFI partition
echo [STEP 1/4] Mounting EFI partition...
mountvol X: /s
if %errorlevel% neq 0 (
    echo [ERROR] Failed to mount EFI partition!
    pause
    exit /b 1
)
echo [OK] EFI mounted on X:
echo.

REM Create backup of current bootmgfw.efi (safety)
echo [STEP 2/4] Creating additional safety backup...
set TIMESTAMP=%date:~-4%%date:~3,2%%date:~0,2%_%time:~0,2%%time:~3,2%
set TIMESTAMP=%TIMESTAMP: =0%
copy "X:\EFI\Microsoft\Boot\bootmgfw.efi" "%BACKUP_DIR%\bootmgfw_predeploy_%TIMESTAMP%.efi" >nul
echo [OK] Pre-deploy backup created
echo.

REM Copy original bootmgfw.efi to EFI partition (for hyper-reV to restore)
echo [STEP 3/4] Copying original bootloader for hyper-reV...
copy "%BACKUP_DIR%\bootmgfw_original.efi" "X:\EFI\Microsoft\Boot\bootmgfw_original.efi" >nul
if %errorlevel% neq 0 (
    echo [ERROR] Failed to copy original bootloader!
    mountvol X: /d
    pause
    exit /b 1
)
echo [OK] Original bootloader copied
echo.

REM Copy hyperv-attachment.dll
echo [STEP 4/4] Deploying hyper-reV...
copy "%BUILD_DIR%\hyperv-attachment.dll" "X:\EFI\Microsoft\Boot\hyperv-attachment.dll" >nul
if %errorlevel% neq 0 (
    echo [ERROR] Failed to copy hyperv-attachment.dll!
    mountvol X: /d
    pause
    exit /b 1
)
echo [OK] hyperv-attachment.dll deployed

REM Replace bootmgfw.efi with uefi-boot.efi
copy "%BUILD_DIR%\uefi-boot.efi" "X:\EFI\Microsoft\Boot\bootmgfw.efi" >nul
if %errorlevel% neq 0 (
    echo [ERROR] Failed to replace bootmgfw.efi!
    echo Attempting to restore backup...
    copy "%BACKUP_DIR%\bootmgfw_original.efi" "X:\EFI\Microsoft\Boot\bootmgfw.efi" >nul
    mountvol X: /d
    pause
    exit /b 1
)
echo [OK] bootmgfw.efi replaced with uefi-boot.efi
echo.

REM Verify deployment
echo [INFO] Verifying deployment...
dir "X:\EFI\Microsoft\Boot\*.efi" "X:\EFI\Microsoft\Boot\*.dll"
echo.

REM Unmount EFI
mountvol X: /d
echo [OK] EFI partition unmounted
echo.

echo =============================================
echo   hyper-reV DEPLOYED SUCCESSFULLY!
echo =============================================
echo.
echo [NEXT STEPS]
echo 1. Reboot your computer
echo 2. hyper-reV will load before Windows
echo 3. After Windows boots, run the cheat
echo.
echo [IF BOOT FAILS]
echo 1. Boot from Windows USB
echo 2. Press Shift+F10 for command prompt
echo 3. Run: recovery-hyper-rev.bat
echo.
echo [TO UNINSTALL]
echo Run: recovery-hyper-rev.bat
echo.
set /p REBOOT="Reboot now? (Y/N): "
if /i "%REBOOT%"=="Y" (
    echo.
    echo Rebooting in 5 seconds...
    shutdown /r /t 5
) else (
    echo.
    echo Remember to reboot to activate hyper-reV!
)
echo.
pause

