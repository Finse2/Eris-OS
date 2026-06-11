nasm Boot.asm -f bin -o bin\bootsect.bin
nasm Kernel_Entry.asm -f elf -o bin\Entry.bin
nasm IDT.asm -f elf -o bin\IDT.bin
nasm ata.asm -f elf -o bin\ata.bin

gcc -m32 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -c main.c -o bin\kernel.o
gcc -m32 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -c filesystem.c -o bin\filesystem.o

REM Link kernel (MinGW ld uses PE format, but we convert to binary)
REM -Ttext 0x1000: code starts at 0x1000
REM Note: MinGW ld doesn't support elf_i386, but PE format works fine for our needs
REM Entry.bin should be first since it contains the entry point
ld -o bin\Kernel.img -Ttext 0x1000 bin\Entry.bin bin\kernel.o bin\filesystem.o bin\IDT.bin bin\ata.bin

REM CRITICAL: Copy ALL sections, not just .text
REM This ensures .data, .bss, and other sections are included
REM Convert PE/COFF to raw binary
objcopy -O binary bin\Kernel.img bin\kernel.bin
copy /b /Y bin\bootsect.bin+bin\kernel.bin bin\os-image

REM Run QEMU
REM The GTK warning about pixbuf is harmless - it's just a theme loading issue
REM Options to suppress it:
REM 1. Redirect stderr (hides all errors - not recommended)
REM 2. Use different display backend: -display sdl or -display curses
REM 3. Set GTK environment variable: set GTK_THEME=Adwaita:dark
REM 4. Just ignore it - it doesn't affect functionality

REM Run QEMU with debug port 0xE9 output written to file
REM -debugcon file:ttemp.log: Saves all debug output to ttemp.log file
REM -global isa-debugcon.iobase=0xe9: Explicitly set port (default is 0xe9, optional)

REM Delete old log file if it exists
if exist ttemp.log del ttemp.log

REM Run QEMU and write debug output to ttemp.log
REM NOTE: On Windows, you may need to use absolute path or ensure file permissions
REM The file will be created/updated when QEMU writes to port 0xE9

REM Method 1: View debug output in terminal (RECOMMENDED FOR DEBUGGING CRASHES)
REM This shows output immediately so we can see where the crash happens
qemu-system-i386 -drive format=raw,file=bin\os-image -debugcon stdio

REM Method 2: Write to file (use after debugging is complete)
REM qemu-system-i386 -drive format=raw,file=bin\os-image -debugcon file:ttemp.log

REM Method 3: If Method 2 doesn't work, try absolute path
REM qemu-system-i386 -drive format=raw,file=bin\os-image -debugcon file:%CD%\ttemp.log

REM After QEMU exits, check if ttemp.log was created and has content
REM If empty, the debug output might not be reaching port 0xE9

REM Alternative: View in terminal instead (uncomment to use)
REM qemu-system-i386 -drive format=raw,file=bin\os-image -debugcon stdio

REM To suppress GTK warning while logging to file:
REM qemu-system-i386 -drive format=raw,file=bin\os-image -debugcon file:ttemp.log 2>nul
