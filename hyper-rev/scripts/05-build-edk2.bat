@echo off
REM ============================================================
REM STEP 5: Build EDK2 Libraries
REM ============================================================
REM This builds the EDK2 UEFI development libraries
REM Required for compiling the uefi-boot module

echo =============================================
echo   hyper-reV Setup - Build EDK2 Libraries
echo =============================================
echo.

REM Set paths
set SCRIPT_DIR=%~dp0
set HYPER_REV_DIR=%SCRIPT_DIR%..\src\hyper-reV
set EDK2_DIR=%HYPER_REV_DIR%\uefi-boot\ext\edk2

REM Check if EDK2 exists
if not exist "%EDK2_DIR%\src" (
    echo [ERROR] EDK2 not found!
    echo Please run 04-clone-repos.bat first
    pause
    exit /b 1
)

REM Check for Visual Studio
where devenv >nul 2>&1
if %errorlevel% neq 0 (
    echo [WARN] Visual Studio devenv not in PATH
    echo Trying to find Visual Studio...
    
    REM Try common VS2022 locations
    set VS_PATH=
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" (
        set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community
    )
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.exe" (
        set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional
    )
    if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.exe" (
        set VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise
    )
    
    if "%VS_PATH%"=="" (
        echo [ERROR] Visual Studio 2022 not found!
        echo Please install Visual Studio 2022 with C++ Desktop Development
        pause
        exit /b 1
    )
    
    echo [OK] Found Visual Studio at: %VS_PATH%
    set PATH=%PATH%;%VS_PATH%\Common7\IDE
)

echo [OK] Visual Studio found
echo.

REM Navigate to EDK2 build directory
cd /d "%EDK2_DIR%\build"

REM Check if solution exists
if not exist "EDK-II.sln" (
    echo [ERROR] EDK-II.sln not found in: %EDK2_DIR%\build
    echo.
    echo Directory contents:
    dir /b
    pause
    exit /b 1
)

echo [STEP 1/2] Building EDK2 libraries (Release x64)...
echo This may take 5-10 minutes...
echo.

REM Build using MSBuild (faster than devenv)
where msbuild >nul 2>&1
if %errorlevel%==0 (
    echo Using MSBuild...
    msbuild EDK-II.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
) else (
    echo Using devenv...
    devenv EDK-II.sln /Build "Release|x64"
)

if %errorlevel% neq 0 (
    echo.
    echo [ERROR] EDK2 build failed!
    echo.
    echo Common fixes:
    echo 1. Make sure Visual Studio C++ tools are installed
    echo 2. Try opening EDK-II.sln manually in VS2022 and building
    echo.
    echo Solution path: %EDK2_DIR%\build\EDK-II.sln
    pause
    exit /b 1
)

echo.
echo [OK] EDK2 Release x64 build complete
echo.

REM Verify output
echo [STEP 2/2] Verifying build output...
set BUILD_OK=1

if not exist "%EDK2_DIR%\build\x64\Release\*.lib" (
    echo [WARN] No .lib files found in Release output
    set BUILD_OK=0
)

if %BUILD_OK%==1 (
    echo [OK] Build output verified
    echo.
    echo Libraries built:
    dir /b "%EDK2_DIR%\build\x64\Release\*.lib" 2>nul
)

echo.
echo =============================================
echo   EDK2 libraries built successfully!
echo =============================================
echo.
echo Next step: Build hyper-reV
echo Run: 06-build-hyper-rev.bat
echo.
pause

