@echo off
setlocal enabledelayedexpansion
chcp 65001 >nul 2>&1
cd /d "%~dp0"

echo.
echo ========================================
echo    EXTERNA CS2 - BUILD SCRIPT
echo ========================================
echo.

REM Check Rust
where cargo >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Rust not found!
    echo Install from: https://rustup.rs
    pause
    exit /b 1
)
echo [OK] Rust found

REM Check Visual Studio
set "VS_PATH="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

if defined VS_PATH (
    echo [OK] Visual Studio 2022 found
) else (
    echo [WARN] Visual Studio 2022 not found - driver build will be skipped
)

REM Check WDK
set "WDK_FOUND=0"
if exist "C:\Program Files (x86)\Windows Kits\10\Include" (
    set "WDK_FOUND=1"
    echo [OK] Windows Driver Kit found
) else (
    echo [WARN] WDK not found - driver build will be skipped
)

REM Check Intel driver
set "INTEL_EXISTS=0"
if exist "cheats\externa-rust-v2\assets\iqvw64e.sys" (
    set "INTEL_EXISTS=1"
    echo [OK] Intel driver found
) else (
    echo [WARN] iqvw64e.sys not found in cheats\externa-rust-v2\assets\
    echo        Download from: https://github.com/TheCruZ/kdmapper/releases
)

echo.
echo ----------------------------------------
echo    STEP 1: Build Kernel Driver
echo ----------------------------------------
echo.

set "DRIVER_BUILT=0"
if defined VS_PATH if "%WDK_FOUND%"=="1" (
    if exist "driver\laithdriver\laithdriver.sln" (
        echo Building driver...
        call "%VS_PATH%"
        msbuild "driver\laithdriver\laithdriver.sln" /p:Configuration=Release /p:Platform=x64 /t:Build /v:minimal
        if exist "driver\laithdriver\x64\Release\laithdriver.sys" (
            echo [OK] Driver built successfully
            set "DRIVER_BUILT=1"
            
            REM Copy to assets
            if not exist "cheats\externa-rust-v2\assets" mkdir "cheats\externa-rust-v2\assets"
            copy /Y "driver\laithdriver\x64\Release\laithdriver.sys" "cheats\externa-rust-v2\assets\" >nul
            echo [OK] Driver copied to assets
        ) else (
            echo [ERROR] Driver build failed
        )
    ) else (
        echo [SKIP] Driver project not found
    )
) else (
    echo [SKIP] VS or WDK not available
)

echo.
echo ----------------------------------------
echo    STEP 2: Build Rust Cheat
echo ----------------------------------------
echo.

cd cheats\externa-rust-v2

REM Decide if we can embed drivers
set "EMBED_FLAG="
if exist "assets\laithdriver.sys" if exist "assets\iqvw64e.sys" (
    echo [OK] Both drivers found - building with embedded drivers
    set "EMBED_FLAG=--features embed_drivers"
) else (
    echo [INFO] Building without embedded drivers
)

echo Building Rust project...
cargo build --release %EMBED_FLAG%
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Rust build failed!
    cd ..\..
    pause
    exit /b 1
)
echo [OK] Rust build successful

cd ..\..

echo.
echo ----------------------------------------
echo    STEP 3: Create Release Package
echo ----------------------------------------
echo.

if not exist "release" mkdir "release"

REM Copy main executable
if exist "cheats\externa-rust-v2\target\release\externa-rust-v2.exe" (
    copy /Y "cheats\externa-rust-v2\target\release\externa-rust-v2.exe" "release\Externa-CS2.exe" >nul
    echo [OK] Externa-CS2.exe copied to release folder
)

REM Copy drivers if they exist and weren't embedded
if exist "cheats\externa-rust-v2\assets\laithdriver.sys" (
    copy /Y "cheats\externa-rust-v2\assets\laithdriver.sys" "release\" >nul
    echo [OK] laithdriver.sys copied
)
if exist "cheats\externa-rust-v2\assets\iqvw64e.sys" (
    copy /Y "cheats\externa-rust-v2\assets\iqvw64e.sys" "release\" >nul
    echo [OK] iqvw64e.sys copied
)

echo.
echo ========================================
echo    BUILD COMPLETE!
echo ========================================
echo.
echo Files in release folder:
dir /b release
echo.
echo To run: release\Externa-CS2.exe
echo.
pause
