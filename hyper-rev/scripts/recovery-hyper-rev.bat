@echo off
REM ============================================================
REM RECOVERY: Restore original Windows bootloader
REM ============================================================
REM Use this if:
REM 1. Windows won't boot after deploying hyper-reV
REM 2. You want to uninstall hyper-reV
REM
REM How to use if Windows won't boot:
REM 1. Boot from Windows Installation USB
REM 2. Click "Repair your computer"
REM 3. Troubleshoot -> Command Prompt
REM 4. Navigate to this script and run it
REM    Or manually run the commands below
REM ============================================================

echo =============================================
echo   hyper-reV RECOVERY SCRIPT
echo =============================================
echo.
echo This will restore the original Windows bootloader.
echo.

REM Try to find the backup
set SCRIPT_DIR=%~dp0
set BACKUP_DIR=%SCRIPT_DIR%..\backup

REM Check if we're running from the normal location
if exist "%BACKUP_DIR%\bootmgfw_original.efi" (
    set BACKUP_FILE=%BACKUP_DIR%\bootmgfw_original.efi
    echo [OK] Found backup at: %BACKUP_FILE%
    goto :do_recovery
)

REM If not found, we might be running from recovery environment
echo [INFO] Backup not found at normal location
echo [INFO] Attempting to locate backup on all drives...
echo.

REM Search for backup on all drives
for %%D in (C D E F G H I J K L M N O P Q R S T U V W X Y Z) do (
    if exist "%%D:\Users" (
        echo Searching %%D: drive...
        for /r "%%D:\Users" %%F in (bootmgfw_original.efi) do (
            if exist "%%F" (
                echo [OK] Found backup: %%F
                set BACKUP_FILE=%%F
                goto :do_recovery
            )
        )
    )
)

REM Still not found, try EFI partition
echo.
echo [INFO] Checking EFI partition...
mountvol X: /s 2>nul
if exist "X:\EFI\Microsoft\Boot\bootmgfw_original.efi" (
    set BACKUP_FILE=X:\EFI\Microsoft\Boot\bootmgfw_original.efi
    echo [OK] Found backup on EFI partition
    goto :do_recovery_efi_mounted
)

REM No backup found
echo.
echo [ERROR] Could not find bootmgfw_original.efi backup!
echo.
echo Manual recovery options:
echo.
echo Option 1: Use Windows Recovery
echo   bootrec /fixboot
echo   bootrec /fixmbr
echo   bootrec /rebuildbcd
echo.
echo Option 2: Copy bootmgfw.efi from another Windows installation
echo.
echo Option 3: Reinstall Windows
echo.
pause
exit /b 1

:do_recovery
REM Mount EFI partition
echo.
echo [STEP 1/3] Mounting EFI partition...
mountvol X: /s
if %errorlevel% neq 0 (
    echo [ERROR] Failed to mount EFI partition!
    echo.
    echo Try manually:
    echo   diskpart
    echo   list vol
    echo   select vol [EFI volume number]
    echo   assign letter=X
    echo   exit
    pause
    exit /b 1
)
echo [OK] EFI partition mounted on X:

:do_recovery_efi_mounted
echo.
echo [STEP 2/3] Restoring original bootloader...
echo Source: %BACKUP_FILE%
echo Target: X:\EFI\Microsoft\Boot\bootmgfw.efi
echo.

copy /Y "%BACKUP_FILE%" "X:\EFI\Microsoft\Boot\bootmgfw.efi"
if %errorlevel% neq 0 (
    echo [ERROR] Failed to restore bootloader!
    pause
    exit /b 1
)
echo [OK] Original bootloader restored

REM Clean up hyper-reV files
echo.
echo [STEP 3/3] Cleaning up hyper-reV files...
if exist "X:\EFI\Microsoft\Boot\hyperv-attachment.dll" (
    del "X:\EFI\Microsoft\Boot\hyperv-attachment.dll"
    echo [OK] Removed hyperv-attachment.dll
)
if exist "X:\EFI\Microsoft\Boot\bootmgfw_original.efi" (
    del "X:\EFI\Microsoft\Boot\bootmgfw_original.efi"
    echo [OK] Removed bootmgfw_original.efi
)

REM Unmount
echo.
mountvol X: /d 2>nul
echo [OK] EFI partition unmounted
echo.

echo =============================================
echo   RECOVERY COMPLETE!
echo =============================================
echo.
echo The original Windows bootloader has been restored.
echo hyper-reV has been removed.
echo.
echo You can now reboot safely.
echo.
set /p REBOOT="Reboot now? (Y/N): "
if /i "%REBOOT%"=="Y" (
    shutdown /r /t 3
)
pause

