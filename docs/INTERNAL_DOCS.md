# ErisOS Internal Docs

This document maps the actual boot/runtime path from current source files.

Back to [README](README.md)

## 1. Boot Path

- BIOS loads sector 0 at `0x7C00`
- `Boot.asm` reads kernel sectors using CHS (`int 13h`) starting from sector 2
- Kernel load destination: `0x1000:0x0000` (`0x10000` physical)
- Sector count is generated into `boot_sectors.inc` by `compile.bat`
- Boot markers:
  - `B`: bootloader entered
  - `L`: kernel sectors loaded
  - `C`: ready to switch
  - `X`: disk read error
- Bootloader enables protected mode and `call 0x10000`

## 2. Kernel Entry And Init

- `Kernel_Entry.asm` calls `_start` (`start()` in `main.c`)
- `start()` flow:
  - text setup + banners
  - `fs_init()`
  - `initIDT()` / PIC remap / keyboard+mouse IRQ setup
  - `paging_init()`
  - `heap_init()`
  - `pit_init()` with `PIT_HZ = 200`
  - `ne2k_init()` and optional DHCP
  - prints prompt `OS0 >`
  - starts graphical setup/login/desktop state when appropriate
  - main loop: setup/login/desktop tick handling, then `hlt`

## 3. Memory Layout

- Kernel link/load base: `0x10000` (`link.ld`)
- Program `EXEC` load address: `0x50000`
- Heap region: `0x60000` to `0x8FFFF` (192KB)
- Boot stack set by loader: `0x90000`
- VGA text buffer: `0xB8000`
- Desktop backbuffer: `0x400000` and above
- Paging identity-maps the low kernel/runtime area plus the desktop backbuffer region
- VESA LFB candidates: `0xE0000000` and `0xFD000000`

## 4. Display And Desktop

- Text mode: VGA `80x25`
- VESA mode command path supports automatic safe 32-bit mode selection plus manual `1280x720`, `1024x768`, `800x600`, and `640x480`
- Auto mode tries `1280x720`, `1024x768`, `960x720`, `800x600`, then `640x480`
- First launch setup, login, loading, and desktop windows are centered from the active VESA width/height
- `DESKTOP` runs a Win95-style shell UI and in-window terminal bridge
- `CMDRST` returns from the graphical terminal bridge to the text UI
- `ESC` handling in graphics mode:
  - first closes overlays/dialogs
  - if nothing to close, exits to text shell

Persistent UI state lives in `ERISCFG.TXT`:

- `THEME`: light/classic or dark
- `FPS`: Eris login animation speed
- `MOUSE`: mouse sensitivity
- `RES`: display preference
- `TZ`: timezone offset from UTC

The desktop taskbar time reads the PC CMOS/RTC clock and applies `TZ`.

## 5. Input And Interrupts

- IRQ0: PIT tick increments `pit_ticks`
- IRQ1: keyboard handler with:
  - command history (`Up`/`Down`)
  - 100-line scrollback view
  - editor routing when active
- IRQ12: PS/2 mouse with Intellimouse wheel detection (4-byte packets)
- IRQ11: NE2000 interrupt ack path when NIC is on IRQ11

## 6. Filesystem Runtime

- `bin\hdd.img` is the filesystem disk (10MB)
- Filesystem metadata on sectors:
  - 1 = FAT
  - 2 = directory table
  - 3+ = file data
- Max files: 16
- Max filename length: 12
- `EXEC` requires `CHMODX` (executable flag)

Important system files:

- `ACCOUNT.TXT`: setup-created login username/password
- `ERISCFG.TXT`: setup/settings-created desktop configuration

If either file is missing, the next graphical boot path enters first launch setup.

## 7. Networking Runtime

- NIC target: RTL8029 (`vendor 0x10EC`, `device 0x8029`)
- Driver in `ne2000.c` / `ne2000.h`
- Protocol/shell integration in `extra.h`
- DHCP attempted at boot if NIC init succeeds

## 8. Build Pipeline (`compile.bat`)

1. Assemble: `Kernel_Entry.asm`, `IDT.asm`, `ata.asm`
2. Compile: `main.c`, `filesystem.c`, `ne2000.c`
3. Link to `bin\Kernel.img`
4. `objcopy` -> `bin\kernel.bin`
5. Compute sector count -> `boot_sectors.inc`
6. Assemble `Boot.asm` -> `bin\bootsect.bin`
7. Concatenate to `bin\os-image-new`
8. Pad image to 1.44MB
9. Create `bin\hdd.img` if missing
10. Launch QEMU with floppy + IDE disk + `ne2k_pci`

## 9. Source Of Truth

If docs and behavior differ, trust in this order:

1. `Boot.asm`, `Kernel_Entry.asm`, `main.c`, `extra.h`, `filesystem.c`, `ne2000.c`
2. `compile.bat`, `link.ld`
3. markdown docs
