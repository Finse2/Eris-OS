# ErisOS Documentation

ErisOS is a 32-bit x86 hobby OS written in C and NASM. It boots from a floppy image, provides a text shell, and includes a VESA desktop UI.

## What Is Current

- Boot path: BIOS -> `Boot.asm` -> protected mode -> `Kernel_Entry.asm` -> `start()` in `main.c`
- Shell prompt: `OS0 >`
- Graphics: VESA 32-bit desktop with automatic safe mode selection (`DESKTOP`)
- Desktop settings: persistent theme, login animation speed, mouse sensitivity, display preference, and timezone (`ERISCFG.TXT`)
- Login account: file-backed default account (`ACCOUNT.TXT`, default `admin` / `eris`)
- First launch setup: creates account/settings, picks theme, resolution, and timezone
- Storage: ATA PIO filesystem on `bin\hdd.img`
- Networking: RTL8029/NE2000 driver with ARP/DHCP/ICMP/UDP/DNS/TCP client utilities
- Built-in editor + built-in assembler

## Build And Run

### Prerequisites

- `nasm`
- 32-bit capable `gcc`
- `ld`, `objcopy`
- `qemu-system-i386`

### Command

```bat
compile.bat
```

`compile.bat` assembles/compiles/links, creates `bin\os-image-new` (1.44MB floppy image), ensures `bin\hdd.img` (10MB), then launches QEMU.

## Boot Artifacts

- `bin\bootsect.bin`: boot sector
- `bin\kernel.bin`: raw kernel payload
- `bin\Kernel.img`: linked kernel ELF
- `bin\os-image-new`: boot floppy image
- `bin\hdd.img`: ATA filesystem disk image

## ISO / Real Hardware

For GRUB ISO testing:

```bat
build_iso.bat
```

The ISO path uses real-hardware safe mode, which blocks ATA writes to physical IDE disks. See `docs/REAL_HARDWARE.md` before booting on a computer.

## Shell Commands

### Display

- `CLS`
- `COLOR <scheme>`
- `COLORBG <color>`
- `COLORFG <color>`
- `VGAMODE <AUTO|VESA|1280|1024|800|640|TEST|TEXT>`
- Settings app: change theme, display mode, animation speed, mouse speed, and timezone
- `DESKTOP [START|MYCOMPUTER|RECYCLE|DOCS|NOTEPAD|IE|CONTROL|TERMINAL|HELP|SHUTDOWN]`
- `CMDRST`: return from the graphical terminal bridge to the text UI
- Legacy: `COLORA`, `COLORB`, `COLORC`, `COLORDEF`

### System

- `HELP [command]`
- `SYSINFO`
- `CPUINFO`
- `MEMINFO`
- `PGINFO`
- `HEAP`
- `UPTIME`
- Legacy/debug: `VID`, `HI`, `PUT`, `GET`, `MOUSE`

### File/Program

- `CREATE <filename>`
- `DELETE <filename>`
- `READ <filename>`
- `WRITE <filename> <data>`
- `LIST`
- `SIZE <filename>`
- `CHMODX <filename>`
- `EXEC <filename>`
- `SECTOR`
- `INSTALL <HELLO|BEEP|CLEAR|TEST>`

### Audio

- `BEEP [freq] [ms]`
- `MUTE`

### Networking

- `NETINFO`, `NETTX`, `NETRX`
- `ARPWHO <IPv4>`, `ARPCACHE`, `IPTX <IPv4>`
- `DHCP`, `PING <IPv4>`
- `UDPBIND <port>`, `UDPSEND <src_port> <dst_ip> <dst_port> <message>`, `UDPRECV <port>`, `UDPSOCK`
- `TCPCONN <dst_ip> <dst_port>`, `TCPSEND <text>`, `TCPRECV`, `TCPCLOSE`, `TCPSTAT`
- `TCPGET <dst_ip> <dst_port> [path]`
- `DNS <hostname> [dns_server_ip]`
- `HTTPGET <host_or_ip> [path] [port]`
- `WEB <host_or_url> [path] [port]`
- Desktop Internet app: plain HTTP browsing/search with DHCP button and IP/DNS status

### Editor/Assembler

- `EDIT <filename>`
- `ASM <source.asm> <output.bin>`
- `HELP ASM [instruction]`

## First Boot Workflow

1. Run `compile.bat`.
2. If `ACCOUNT.TXT` or `ERISCFG.TXT` is missing, complete first launch setup.
3. Log in with the account created during setup.
4. Use the desktop, Settings app, or Terminal.
5. Return to the text shell from the desktop terminal with `CMDRST`, or leave the desktop with `ESC`.

First launch setup appears when `ACCOUNT.TXT` or `ERISCFG.TXT` is missing from the ATA filesystem. Delete either file from ErisOS to test setup again.

For a text-only test path:

1. At `OS0 >`, run `HELP`.
2. Optional graphics test: `VGAMODE TEST`.
3. Launch desktop manually: `DESKTOP`.
4. Return to shell: `ESC`.

## Documentation Index

- `docs/INTERNAL_DOCS.md`: boot/runtime architecture
- `docs/DESKTOP_AND_SETUP.md`: first setup, login, desktop, settings, display behavior
- `docs/API_AND_FILESYSTEM.md`: filesystem + KernelAPI + executable model
- `docs/NETWORKING.md`: networking behavior and command usage
- `docs/ASSEMBLER_REF.md`: built-in assembler/editor reference
- `docs/CODE_EXAMPLES.md`: runnable shell/ASM examples
- `docs/TROUBLESHOOTING.md`: common QEMU, setup, display, and networking checks
- `docs/REAL_HARDWARE.md`: GRUB ISO and real-machine safety notes
- `docs/GLOSSARY.md`: terms and abbreviations
