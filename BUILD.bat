@echo off
chcp 65001 >nul
setlocal enabledelayedexpansion

echo ╔══════════════════════════════════════════════════════════════╗
echo ║           Externa CS2 ESP - Full Build Script                ║
echo ╠══════════════════════════════════════════════════════════════╣
echo ║  This script builds:                                         ║
echo ║  1. Kernel Driver (laithdriver.sys)                         ║
echo ║  2. Rust Cheat with embedded drivers                        ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

:: Check for Visual Studio
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] Visual Studio not found!
    echo Download from: https://visualstudio.microsoft.com/
    pause
    exit /b 1
)

:: Find MSBuild
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD=%%i"
)

if not defined MSBUILD (
    echo [ERROR] MSBuild not found!
    pause
    exit /b 1
)
echo [OK] Found MSBuild: %MSBUILD%

:: Check for WDK
set "WDK_PATH=%ProgramFiles(x86)%\Windows Kits\10\Include"
if not exist "%WDK_PATH%" (
    echo [WARNING] Windows Driver Kit not found!
    echo Download from: https://docs.microsoft.com/windows-hardware/drivers/download-the-wdk
    echo.
    echo Skipping driver build...
    goto :skip_driver
)
echo [OK] Found WDK

:: Build Driver
echo.
echo ══════════════════════════════════════════════════════════════
echo   Building Kernel Driver...
echo ══════════════════════════════════════════════════════════════
cd /d "%~dp0laith-km-driver"
"%MSBUILD%" Driver.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Driver build failed!
    goto :skip_driver
)

:: Find built driver
set "DRIVER_SYS="
for /r %%f in (*.sys) do (
    set "DRIVER_SYS=%%f"
)

if defined DRIVER_SYS (
    echo [OK] Driver built: %DRIVER_SYS%
    
    :: Copy to assets
    if not exist "%~dp0cheats\externa-rust-v2\assets" mkdir "%~dp0cheats\externa-rust-v2\assets"
    copy "%DRIVER_SYS%" "%~dp0cheats\externa-rust-v2\assets\laithdriver.sys" >nul
    echo [OK] Copied to assets folder
) else (
    echo [WARNING] Driver .sys file not found
)

:skip_driver

:: Check for Intel driver
echo.
if not exist "%~dp0cheats\externa-rust-v2\assets\iqvw64e.sys" (
    echo [WARNING] Intel driver (iqvw64e.sys) not found!
    echo.
    echo Download from: https://github.com/TheCruZ/kdmapper/releases
    echo Place in: cheats\externa-rust-v2\assets\iqvw64e.sys
    echo.
    echo Building without embedded drivers...
    set "BUILD_EMBEDDED=false"
) else (
    echo [OK] Intel driver found
    set "BUILD_EMBEDDED=true"
)

:: Check for Rust
echo.
where cargo >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Rust not found!
    echo Install from: https://rustup.rs/
    pause
    exit /b 1
)
echo [OK] Found Rust

:: Build Rust Cheat
echo.
echo ══════════════════════════════════════════════════════════════
echo   Building Rust Cheat...
echo ══════════════════════════════════════════════════════════════
cd /d "%~dp0cheats\externa-rust-v2"

:: Check if we can embed
set "LAITH_EXISTS=false"
set "INTEL_EXISTS=false"
if exist "assets\laithdriver.sys" set "LAITH_EXISTS=true"
if exist "assets\iqvw64e.sys" set "INTEL_EXISTS=true"

if "%LAITH_EXISTS%"=="true" if "%INTEL_EXISTS%"=="true" (
    echo [INFO] Building with EMBEDDED drivers...
    cargo build --release --features embed_drivers
    set "OUTPUT_NAME=Externa-CS2-FULL.exe"
) else (
    echo [INFO] Building STANDARD version (syscall mode)...
    cargo build --release
    set "OUTPUT_NAME=Externa-CS2.exe"
)

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Rust build failed!
    pause
    exit /b 1
)

:: Copy result
if not exist "%~dp0release" mkdir "%~dp0release"
copy "target\release\externa-rust-v2.exe" "%~dp0release\%OUTPUT_NAME%" >nul

:: Also copy drivers if available
if exist "assets\laithdriver.sys" (
    if not exist "%~dp0release\driver" mkdir "%~dp0release\driver"
    copy "assets\laithdriver.sys" "%~dp0release\driver\" >nul
)
if exist "assets\iqvw64e.sys" (
    if not exist "%~dp0release\driver" mkdir "%~dp0release\driver"
    copy "assets\iqvw64e.sys" "%~dp0release\driver\" >nul
)

echo.
echo ╔══════════════════════════════════════════════════════════════╗
echo ║                    BUILD COMPLETE!                           ║
echo ╠══════════════════════════════════════════════════════════════╣
echo ║  Output: release\%OUTPUT_NAME%
echo ║                                                              ║
echo ║  Usage:                                                      ║
echo ║  1. Start CS2                                                ║
echo ║  2. Run %OUTPUT_NAME% as Administrator
echo ║  3. Press INSERT to toggle ESP                               ║
echo ╚══════════════════════════════════════════════════════════════╝
echo.

:: Open release folder
explorer "%~dp0release"

pause

