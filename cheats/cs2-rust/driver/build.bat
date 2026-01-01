@echo off
echo ============================================
echo   EXTERNA KERNEL DRIVER BUILD
echo ============================================
echo.

:: Check for Rust
where cargo >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Rust not found! Install from https://rustup.rs
    pause
    exit /b 1
)

:: Check for nightly
rustup run nightly rustc --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [*] Installing Rust nightly...
    rustup install nightly
    rustup component add rust-src --toolchain nightly
)

echo [*] Building driver...
cargo +nightly build --release -Z build-std=core,alloc --target x86_64-pc-windows-msvc

if %errorlevel% equ 0 (
    echo.
    echo ============================================
    echo   BUILD SUCCESSFUL!
    echo ============================================
    echo.
    echo Driver location:
    echo   target\x86_64-pc-windows-msvc\release\externa_driver.sys
    echo.
) else (
    echo.
    echo [ERROR] Build failed!
    echo.
    echo Make sure you have installed:
    echo   1. Visual Studio 2022 with C++ workload
    echo   2. Windows SDK
    echo   3. Windows Driver Kit (WDK)
    echo.
)

pause

