@echo off
cd /d "%~dp0"
echo Building Externa Rust V2 (Direct3D 11 Edition)...
cargo build --release
echo.
echo Done. Binary is in target/release/externa-cs2-v2.exe
pause

