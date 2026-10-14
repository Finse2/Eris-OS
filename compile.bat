@echo off
nasm Kernel_Entry.asm -f elf -o bin\Entry.bin
nasm IDT.asm -f elf -o bin\IDT.bin
nasm ata.asm -f elf -o bin\ata.bin

gcc -m32 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -c main.c -o bin\kernel.o
gcc -m32 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -c filesystem.c -o bin\filesystem.o
gcc -m32 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -c ne2000.c -o bin\ne2000.o

ld -T link.ld -o bin\Kernel.img bin\Entry.bin bin\kernel.o bin\filesystem.o bin\ne2000.o bin\IDT.bin bin\ata.bin

objcopy -O binary bin\Kernel.img bin\kernel.bin

for /f %%a in ('powershell -NoProfile -Command "(Get-Item -Path bin\\kernel.bin).Length"') do set KSIZE=%%a
set /a KSECT=(%KSIZE%+511)/512
echo %%define SECTOR_COUNT %KSECT%> boot_sectors.inc

nasm Boot.asm -f bin -o bin\bootsect.bin
copy /b /Y bin\bootsect.bin+bin\kernel.bin bin\os-image-new

REM Pad to 1.44MB floppy size for boot image
powershell -NoProfile -Command "$f=[System.IO.File]::OpenWrite('bin\os-image-new'); $f.SetLength(1474560); $f.Close()"

REM Create a separate 10MB hard disk image for ATA filesystem
if not exist bin\hdd.img (
    powershell -NoProfile -Command "$f=[System.IO.File]::Create('bin\hdd.img'); $f.SetLength(10485760); $f.Close()"
)

REM Boot from floppy, but add hard disk for ATA filesystem access
qemu-system-i386 -vga std -drive file=bin\os-image-new,format=raw,index=0,if=floppy -drive file=bin\hdd.img,format=raw,index=0,if=ide -netdev user,id=net0 -device ne2k_pci,netdev=net0 -boot a
