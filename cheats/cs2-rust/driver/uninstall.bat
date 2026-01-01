@echo off
echo ============================================
echo   EXTERNA DRIVER UNINSTALLER
echo ============================================
echo.

:: Check for admin rights
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Run as Administrator!
    pause
    exit /b 1
)

set SERVICE_NAME=ExternaDrv

echo [*] Stopping driver...
sc stop %SERVICE_NAME%

echo [*] Removing service...
sc delete %SERVICE_NAME%

echo.
echo ============================================
echo   DRIVER UNINSTALLED
echo ============================================
echo.
pause

