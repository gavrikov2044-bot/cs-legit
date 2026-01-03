@echo off
REM ============================================================
REM STEP 4: Clone hyper-reV and all submodules
REM ============================================================
REM This clones the main repository and required dependencies

echo =============================================
echo   hyper-reV Setup - Clone Repositories
echo =============================================
echo.

REM Check for git
where git >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Git is not installed!
    echo Please install Git from: https://git-scm.com/
    pause
    exit /b 1
)
echo [OK] Git found
echo.

REM Set paths
set SCRIPT_DIR=%~dp0
set SRC_DIR=%SCRIPT_DIR%..\src
set HYPER_REV_DIR=%SRC_DIR%\hyper-reV

REM Create src directory
if not exist "%SRC_DIR%" mkdir "%SRC_DIR%"

REM Clone hyper-reV main repository
echo [STEP 1/4] Cloning hyper-reV main repository...
if exist "%HYPER_REV_DIR%" (
    echo [INFO] hyper-reV already cloned, pulling updates...
    cd /d "%HYPER_REV_DIR%"
    git pull
) else (
    cd /d "%SRC_DIR%"
    git clone https://github.com/noahware/hyper-reV.git
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to clone hyper-reV!
        echo.
        echo If GitHub is blocked, download manually from:
        echo https://github.com/noahware/hyper-reV/archive/refs/heads/main.zip
        pause
        exit /b 1
    )
)
echo [OK] hyper-reV cloned
echo.

REM Navigate to hyper-reV directory
cd /d "%HYPER_REV_DIR%"

REM Clone EDK2 submodule
echo [STEP 2/4] Cloning EDK2 (UEFI development kit)...
if not exist "uefi-boot\ext\edk2\src" (
    git clone https://github.com/ionescu007/edk2.git uefi-boot/ext/edk2/src
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to clone EDK2!
        pause
        exit /b 1
    )
) else (
    echo [INFO] EDK2 already cloned
)
echo [OK] EDK2 cloned
echo.

REM Clone OpenSSL submodule
echo [STEP 3/4] Cloning OpenSSL...
if not exist "uefi-boot\ext\openssl" (
    git clone --branch OpenSSL_1_1_0-stable https://github.com/openssl/openssl.git uefi-boot/ext/openssl
    if %errorlevel% neq 0 (
        echo [ERROR] Failed to clone OpenSSL!
        pause
        exit /b 1
    )
) else (
    echo [INFO] OpenSSL already cloned
)
echo [OK] OpenSSL cloned
echo.

REM Verify structure
echo [STEP 4/4] Verifying repository structure...
set MISSING=0

if not exist "uefi-boot" (
    echo [ERROR] Missing: uefi-boot directory
    set MISSING=1
)
if not exist "hyperv-attachment" (
    echo [ERROR] Missing: hyperv-attachment directory
    set MISSING=1
)
if not exist "usermode-app" (
    echo [ERROR] Missing: usermode-app directory
    set MISSING=1
)
if not exist "uefi-boot\ext\edk2\src" (
    echo [ERROR] Missing: EDK2 source
    set MISSING=1
)
if not exist "uefi-boot\ext\openssl" (
    echo [ERROR] Missing: OpenSSL source
    set MISSING=1
)

if %MISSING%==1 (
    echo.
    echo [ERROR] Some components are missing!
    pause
    exit /b 1
)

echo [OK] All components present
echo.
echo Repository structure:
echo.
dir /b
echo.

echo =============================================
echo   Repositories cloned successfully!
echo =============================================
echo.
echo Location: %HYPER_REV_DIR%
echo.
echo Next step: Build EDK2 libraries
echo Run: 05-build-edk2.bat
echo.
pause

