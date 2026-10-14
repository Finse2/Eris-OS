#!/usr/bin/env sh
set -eu

NASM="${NASM:-nasm}"
GRUB_MKRESCUE="${GRUB_MKRESCUE:-grub-mkrescue}"

if [ -n "${CC:-}" ]; then
  :
elif command -v i686-elf-gcc >/dev/null 2>&1; then
  CC=i686-elf-gcc
else
  CC=gcc
fi

LD="${LD:-$CC}"

mkdir -p bin iso/boot/grub

"$NASM" grub_entry.asm -f elf32 -o bin/grub_entry.o
"$NASM" IDT.asm -f elf32 -o bin/IDT.grub.o
"$NASM" ata.asm -f elf32 -DERISOS_REAL_HW_SAFE -o bin/ata.grub.o

CFLAGS="-m32 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -fleading-underscore -fno-asynchronous-unwind-tables"
"$CC" $CFLAGS -c main.c -o bin/kernel.grub.o
"$CC" $CFLAGS -c filesystem.c -o bin/filesystem.grub.o
"$CC" $CFLAGS -c ne2000.c -o bin/ne2000.grub.o

"$LD" -m32 -nostdlib -no-pie -Wl,-T,link_grub.ld -Wl,--build-id=none \
  -o bin/erisos.elf \
  bin/grub_entry.o bin/kernel.grub.o bin/filesystem.grub.o bin/ne2000.grub.o bin/IDT.grub.o bin/ata.grub.o

if command -v grub-file >/dev/null 2>&1; then
  grub-file --is-x86-multiboot bin/erisos.elf
fi

cp bin/erisos.elf iso/boot/erisos.elf
cp grub/grub.cfg iso/boot/grub/grub.cfg

"$GRUB_MKRESCUE" -o bin/ErisOS.iso iso

echo "Built bin/ErisOS.iso"
