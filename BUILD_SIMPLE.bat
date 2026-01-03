@echo off
cd /d "%~dp0"

echo.
echo Building Externa CS2 (Simple Mode)...
echo.

cd cheats\externa-rust-v2

cargo build --release
if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo.
echo Build successful!
echo.
echo EXE location:
echo   %cd%\target\release\externa-rust-v2.exe
echo.

pause

