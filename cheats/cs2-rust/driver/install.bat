@echo off
echo ============================================
echo   EXTERNA DRIVER INSTALLER
echo ============================================
echo.

:: Check for admin rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Run as Administrator!
    echo Right-click and select "Run as administrator"
    pause
    exit /b 1
)

set DRIVER_PATH=%~dp0target\x86_64-pc-windows-msvc\release\externa_driver.sys
set SERVICE_NAME=ExternaDrv

:: Check if driver exists
if not exist "%DRIVER_PATH%" (
    echo [ERROR] Driver not found!
    echo Run build.bat first.
    pause
    exit /b 1
)

echo [*] Stopping existing service...
sc stop %SERVICE_NAME% >nul 2>&1
sc delete %SERVICE_NAME% >nul 2>&1
timeout /t 2 >nul

echo [*] Creating service...
sc create %SERVICE_NAME% type= kernel binPath= "%DRIVER_PATH%"
if %errorlevel% neq 0 (
    echo [ERROR] Failed to create service!
    echo.
    echo Try enabling Test Mode:
    echo   bcdedit /set testsigning on
    echo   (restart required)
    pause
    exit /b 1
)

echo [*] Starting driver...
sc start %SERVICE_NAME%
if %errorlevel% neq 0 (
    echo [ERROR] Failed to start driver!
    echo.
    echo Possible issues:
    echo   1. Test Mode not enabled
    echo   2. Driver not signed
    echo   3. WDK not installed correctly
    pause
    exit /b 1
)

echo.
echo ============================================
echo   DRIVER INSTALLED SUCCESSFULLY!
echo ============================================
echo.
echo Service: %SERVICE_NAME%
echo Status: Running
echo.
pause

