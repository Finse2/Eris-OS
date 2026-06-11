# ErisOS

**ErisOS** is a tiny 32-bit x86 operating system built entirely from scratch. It's designed for learning low-level systems programming and demonstrates the complete journey from power-on to a working command-line environment.

---

## Introduction

### What is ErisOS?

ErisOS is an educational operating system that shows how computers work at the lowest level. Unlike modern operating systems that hide complexity behind layers of abstraction, ErisOS exposes everything - from the moment the CPU starts executing code to how characters appear on screen.

### Why Build an OS from Scratch?

Building an operating system teaches you:

- **How computers really start** - The BIOS, bootloaders, and the transition from 16-bit to 32-bit mode
- **How software talks to hardware** - Direct port I/O, memory-mapped devices, and interrupt handling
- **How operating systems provide services** - File systems, program loading, and system calls
- **How everything connects** - The relationship between assembly, C, linkers, and the final binary

### Design Philosophy

1. **No external dependencies** - Everything is written from scratch. No libc, no runtime libraries.
2. **Real hardware interaction** - Direct communication with VGA, keyboard, mouse, disk, and speaker.
3. **Complete visibility** - Every part of the system is readable and understandable.
4. **Self-contained development** - You can write, assemble, and run programs entirely within ErisOS.

---

## Features at a Glance

| Component         | Status  | Description 
|-------------------|---------|-------------
| Bootloader        | Working | Loads kernel from floppy, switches to protected mode |
| Protected Mode    | Working | Full 32-bit execution environment |
| VGA Display       | Working | 80x25 text mode with 16 colors |
| Keyboard          | Working | Full US layout with Shift support |
| Mouse             | Working | PS/2 mouse with cursor display and scroll wheel |
| PC Speaker        | Working | Programmable frequency and duration |
| Disk Driver       | Working | ATA PIO for reading/writing sectors |
| Filesystem        | Working | Simple FAT-like structure (16 files, 64 KB) |
| Networking        | Working | RTL8029/NE2000 driver, ARP, DHCP, ICMP ping, DNS resolver, UDP commands, expanded TCP client commands, and HTTP client (`HTTPGET`) |
| Program Execution | Working | Load and run flat binary programs |
| Text Editor       | Working | Full-screen editor for writing code |
| Assembler         | Working | Built-in x86 assembler with conditional jumps |

---

## Getting Started

### Prerequisites

| Tool | Purpose |
|------|---------|
| **NASM** | Assembles the bootloader and assembly modules |
| **GCC (32-bit)** | Compiles the C kernel code |
| **GNU LD** | Links all object files together |
| **GNU objcopy** | Converts ELF to raw binary |
| **QEMU** | Emulates an x86 PC to run ErisOS |

### Building and Running

```batch
compile.bat
```

This script does everything automatically:
1. Assembles all assembly files
2. Compiles the C code
3. Links everything together
4. Creates the boot image
5. Launches QEMU

If the build fails, check the troubleshooting section in [INTERNAL_DOCS.md](INTERNAL_DOCS.md#troubleshooting), especially for:
- `PIT_HZ undeclared`
- `undefined reference to isr0_Handler`
- `objcopy: 'bin\\Kernel.img': No such file`

### First Boot

When ErisOS starts, you'll see:

1. **Boot markers** - `AB12345678C` showing each boot stage completing
2. **Loading animation** - A brief progress indicator (loads fast enough you won't see it)
3. **Welcome message** - The ErisOS banner
4. **Command prompt** - Ready for your input

Try these commands to explore:

```
HELP             ; See all available commands
SYSINFO          ; See system information
LIST             ; Show files on disk
```

### Quick Command Reference

| Category      | Commands |
|---------------|----------|
| **Display**   | `CLS`, `COLOR <scheme>`, `COLORBG <color>`, `COLORFG <color>`, `VGAMODE <VESA|TEST|TEXT>` |
| **System**    | `SYSINFO`, `CPUINFO`, `MEMINFO`, `PGINFO`, `HEAP`, `UPTIME` |
| **Files**     | `LIST`, `CREATE`, `DELETE`, `READ`, `WRITE`, `SIZE` |
| **Programs**  | `EXEC <file>`, `CHMODX <file>`, `INSTALL <prog>` |
| **Dev Tools** | `EDIT <file>`, `ASM <src> <out>`, `HELP ASM <instr>` |
| **Network**   | `NETINFO`, `NETTX`, `NETRX`, `ARPWHO <ip>`, `ARPCACHE`, `IPTX <ip>`, `DHCP`, `PING <ip>`, `DNS`, `UDPBIND`, `UDPSEND`, `UDPRECV`, `UDPSOCK`, `TCPCONN`, `TCPSEND`, `TCPRECV`, `TCPCLOSE`, `TCPSTAT`, `TCPGET`, `HTTPGET` |
| **Audio**     | `BEEP [freq] [ms]`, `MUTE` |
| **Utilities** | `HELP [cmd]`, `MOUSE`, `SECTOR`, `PUT`, `GET` |

**Color schemes:** `MATRIX`, `TERMINAL`, `AMBER`, `DOS`, `MIDNIGHT`, `OCEAN`, `PAPER`, `BLUEPRINT`, `FIRE`, `ICE`, and more.

---

## The Complete Boot Flow

```
Power On
    |
    v
BIOS loads boot sector (512 bytes) at 0x7C00
    |
    v
Boot.asm: Print 'A', 'B' - bootloader starting
    |
    v
Boot.asm: Load 143 kernel sectors (prints '1' through '8')
    |
    v
Boot.asm: Set up GDT, print 'C'
    |
    v
Boot.asm: Switch to Protected Mode (16-bit -> 32-bit)
    |
    v
Boot.asm: Jump to kernel, print 'D' in video memory
    |
    v
Kernel_Entry.asm: Print 'K', call start()
    |
    v
main.c: Initialize display, IDT, keyboard, mouse, filesystem
    |
    v
Command prompt ready for input
```

> For a detailed stage-by-stage breakdown, see [docs/INTERNAL_DOCS.md](docs/INTERNAL_DOCS.md).

---

## Project Structure

```
ErisOS/
|
|-- Boot.asm            # 16-bit bootloader (loads kernel, switches to protected mode)
|-- Kernel_Entry.asm    # 32-bit kernel entry point (calls start() in main.c)
|-- IDT.asm             # Interrupt handlers (keyboard IRQ1, mouse IRQ12)
|-- ata.asm             # ATA disk driver (PIO read/write sectors)
|-- main.c              # Main kernel (display, commands, initialization)
|-- extra.h             # Extended features (parser, editor, assembler, audio)
|-- filesystem.c        # Filesystem (FAT, directory, file I/O, program loading)
|-- filesystem.h        # Filesystem API declarations
|-- link.ld             # Linker script (kernel at 0x10000)
|-- compile.bat         # Build script (assemble, compile, link, run)
|-- boot_sectors.inc    # Auto-generated kernel sector count
|
|-- docs/               # Documentation
|   |-- INTERNAL_DOCS.md      # Boot process, memory map, hardware, build system
|   |-- ASSEMBLER_REF.md      # x86 instruction tables, ASCII, editor, assembler
|   |-- API_AND_FILESYSTEM.md # Kernel API, filesystem, writing programs
|   |-- CODE_EXAMPLES.md      # Practical assembler code examples to learn from
|   |-- GLOSSARY.md           # Definitions for OS development terms
|
|-- bin/                # Build output
    |-- os-image        # Bootable floppy image (1.44 MB)
    |-- hdd.img         # Hard disk image (10 MB)
    |-- kernel.bin      # Raw kernel binary (~66 KB)
```

---



## Future Plans





- [x] VESA high-resolution graphics (1024x768x32 via `VGAMODE VESA`)
- [ ] Clickable mouse interface
- [ ] BASIC interpreter
