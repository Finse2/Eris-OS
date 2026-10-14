# Real Hardware Boot Notes

The GRUB ISO path builds ErisOS in `ERISOS_REAL_HW_SAFE` mode. In this mode,
the ATA driver does not write to the physical IDE disk.

This matters because the normal QEMU build expects a disposable `bin/hdd.img`,
but on a real computer the same ATA ports can refer to a real disk. Raw writes
to sectors 1, 2, and later data sectors can corrupt partitions or filesystems.

Use `bin/ErisOS.iso` for first real-machine tests. Keep the normal
`compile.bat` floppy plus IDE image workflow for QEMU development.

First launch setup, login accounts, desktop settings, and timezone are stored
in ATA filesystem files (`ACCOUNT.TXT` and `ERISCFG.TXT`). In real-hardware
safe mode, those writes are blocked to avoid touching a physical disk. Treat
QEMU's `bin/hdd.img` as the main persistent development environment unless you
have a dedicated, disposable disk target.

To create the ISO from Windows, run:

```bat
build_iso.bat
```

The script uses WSL and expects these tools inside Linux:

```sh
nasm gcc with 32-bit multilib support, grub-mkrescue, xorriso
```

If you have a dedicated cross compiler, `build_iso.sh` will prefer
`i686-elf-gcc` automatically.
