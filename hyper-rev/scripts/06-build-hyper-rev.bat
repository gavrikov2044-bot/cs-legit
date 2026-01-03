@echo off
REM ============================================================
REM STEP 6: Build hyper-reV (uefi-boot + hyperv-attachment)
REM ============================================================
REM This builds both components of hyper-reV

echo =============================================
echo   hyper-reV Setup - Build hyper-reV
echo =============================================
echo.

REM Set paths
set SCRIPT_DIR=%~dp0
set HYPER_REV_DIR=%SCRIPT_DIR%..\src\hyper-reV
set OUTPUT_DIR=%SCRIPT_DIR%..\build

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Check if hyper-reV exists
if not exist "%HYPER_REV_DIR%\uefi-boot" (
    echo [ERROR] hyper-reV source not found!
    echo Please run 04-clone-repos.bat first
    pause
    exit /b 1
)

REM Check for NASM
where nasm >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] NASM not found in PATH!
    echo Please run 03-install-nasm.ps1 first
    echo Then close and reopen this terminal
    pause
    exit /b 1
)
echo [OK] NASM found: 
nasm -v
echo.

REM Check NASM_PREFIX
if "%NASM_PREFIX%"=="" (
    echo [WARN] NASM_PREFIX not set, attempting to set...
    for /f "tokens=*" %%i in ('where nasm') do set NASM_DIR=%%~dpi
    set NASM_PREFIX=%NASM_DIR%
    echo Set NASM_PREFIX=%NASM_PREFIX%
)

REM Detect CPU architecture
echo [INFO] Detecting CPU architecture...
wmic cpu get caption | findstr /i "Intel" >nul
if %errorlevel%==0 (
    set CPU_ARCH=INTEL
    echo [OK] Detected Intel CPU
) else (
    set CPU_ARCH=AMD
    echo [OK] Detected AMD CPU
)
echo.

REM Configure architecture in hyperv-attachment
echo [STEP 1/4] Configuring for %CPU_ARCH%...
set ARCH_CONFIG=%HYPER_REV_DIR%\hyperv-attachment\src\arch_config.h

if not exist "%ARCH_CONFIG%" (
    echo [ERROR] arch_config.h not found!
    pause
    exit /b 1
)

REM Backup original config
copy "%ARCH_CONFIG%" "%ARCH_CONFIG%.bak" >nul

REM Set architecture (Intel or AMD)
if "%CPU_ARCH%"=="INTEL" (
    echo Enabling Intel configuration...
    powershell -Command "(Get-Content '%ARCH_CONFIG%') -replace '//\s*#define\s+_INTELMACHINE', '#define _INTELMACHINE' | Set-Content '%ARCH_CONFIG%'"
    powershell -Command "(Get-Content '%ARCH_CONFIG%') -replace '^#define\s+_AMDMACHINE', '//#define _AMDMACHINE' | Set-Content '%ARCH_CONFIG%'"
) else (
    echo Enabling AMD configuration...
    powershell -Command "(Get-Content '%ARCH_CONFIG%') -replace '^#define\s+_INTELMACHINE', '//#define _INTELMACHINE' | Set-Content '%ARCH_CONFIG%'"
    powershell -Command "(Get-Content '%ARCH_CONFIG%') -replace '//\s*#define\s+_AMDMACHINE', '#define _AMDMACHINE' | Set-Content '%ARCH_CONFIG%'"
)
echo [OK] Architecture configured for %CPU_ARCH%
echo.

REM Find MSBuild
echo [STEP 2/4] Finding MSBuild...
set MSBUILD_PATH=
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>nul`) do (
    set MSBUILD_PATH=%%i
)

if "%MSBUILD_PATH%"=="" (
    echo [WARN] MSBuild not found via vswhere, trying PATH...
    where msbuild >nul 2>&1
    if %errorlevel%==0 (
        set MSBUILD_PATH=msbuild
    ) else (
        echo [ERROR] MSBuild not found!
        echo Please install Visual Studio 2022 with C++ tools
        pause
        exit /b 1
    )
)
echo [OK] MSBuild: %MSBUILD_PATH%
echo.

REM Build uefi-boot
echo [STEP 3/4] Building uefi-boot.efi...
cd /d "%HYPER_REV_DIR%\uefi-boot"

if not exist "uefi-boot.sln" (
    echo [ERROR] uefi-boot.sln not found!
    pause
    exit /b 1
)

"%MSBUILD_PATH%" uefi-boot.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] uefi-boot build failed!
    echo Try opening uefi-boot.sln in Visual Studio manually
    pause
    exit /b 1
)

REM Copy uefi-boot.efi to output
if exist "x64\Release\uefi-boot.efi" (
    copy "x64\Release\uefi-boot.efi" "%OUTPUT_DIR%\" >nul
    echo [OK] uefi-boot.efi built successfully
) else (
    echo [WARN] uefi-boot.efi not found in expected location
    dir /s /b *.efi 2>nul
)
echo.

REM Build hyperv-attachment
echo [STEP 4/4] Building hyperv-attachment.dll...
cd /d "%HYPER_REV_DIR%\hyperv-attachment"

if not exist "hyperv-attachment.sln" (
    echo [ERROR] hyperv-attachment.sln not found!
    pause
    exit /b 1
)

"%MSBUILD_PATH%" hyperv-attachment.sln /p:Configuration=Release /p:Platform=x64 /m /v:minimal
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] hyperv-attachment build failed!
    echo Try opening hyperv-attachment.sln in Visual Studio manually
    pause
    exit /b 1
)

REM Copy hyperv-attachment.dll to output
if exist "x64\Release\hyperv-attachment.dll" (
    copy "x64\Release\hyperv-attachment.dll" "%OUTPUT_DIR%\" >nul
    echo [OK] hyperv-attachment.dll built successfully
) else (
    echo [WARN] hyperv-attachment.dll not found in expected location
    dir /s /b *.dll 2>nul
)
echo.

REM Verify output
echo =============================================
echo   Build Results
echo =============================================
echo.
echo Output directory: %OUTPUT_DIR%
echo.
dir "%OUTPUT_DIR%"
echo.

if exist "%OUTPUT_DIR%\uefi-boot.efi" (
    if exist "%OUTPUT_DIR%\hyperv-attachment.dll" (
        echo [SUCCESS] Both components built!
        echo.
        echo =============================================
        echo   hyper-reV built successfully!
        echo =============================================
        echo.
        echo Architecture: %CPU_ARCH%
        echo.
        echo [WARNING] Next step will MODIFY your bootloader!
        echo Make sure you have:
        echo 1. Created a restore point (step 01)
        echo 2. Backed up bootmgfw.efi (step 02)
        echo 3. A Windows USB for recovery
        echo.
        echo Run: 07-deploy.bat
    ) else (
        echo [ERROR] hyperv-attachment.dll not built
    )
) else (
    echo [ERROR] uefi-boot.efi not built
)
echo.
pause

