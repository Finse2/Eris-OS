@echo off
where wsl >nul 2>nul
if errorlevel 1 (
    echo WSL is required for this ISO build script.
    echo Install a Linux distro with: nasm i686-elf-gcc grub-pc-bin grub-common xorriso
    exit /b 1
)

wsl sh ./build_iso.sh
