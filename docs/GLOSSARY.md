# ErisOS Glossary

Back to [README](README.md)

## Core Terms

- BIOS: firmware that loads the boot sector.
- Boot sector: first 512 bytes of boot media (`0xAA55` signature required).
- Kernel: core OS code loaded after boot sector.
- GDT: segmentation table required for protected-mode transition.
- IDT: interrupt vector table.
- PIC: interrupt controller remapped to avoid CPU exception overlap.

## CPU/Memory

- Real mode: startup 16-bit CPU mode.
- Protected mode: 32-bit mode used by the kernel.
- Flat memory model: unified address space view.
- Stack: call/return and local storage area.
- `cdecl`: caller-pops-stack calling convention, return in `EAX`.
- Backbuffer: memory buffer used for drawing before copying pixels to the visible framebuffer.

## Storage

- CHS: cylinder-head-sector addressing used by BIOS floppy reads.
- FAT: allocation chain table used by ErisOS filesystem.
- Directory table: fixed metadata table for 16 files.
- Sector: 512-byte block.
- ATA PIO: CPU-driven IDE disk I/O mode.

## Graphics/Input

- VGA text mode: `80x25` character mode.
- VESA LFB: linear framebuffer used for desktop graphics.
- VBE/VESA: BIOS graphics extension used to enter high-resolution framebuffer modes.
- LFB: linear framebuffer, a flat pixel-addressable graphics memory area.
- PS/2 mouse packet: 3-byte standard, 4-byte Intellimouse with wheel.

## User Environment

- First launch setup: graphical flow that creates the login account and desktop settings.
- `ACCOUNT.TXT`: filesystem-backed login username/password file.
- `ERISCFG.TXT`: filesystem-backed desktop settings file.
- RTC/CMOS: PC clock hardware used for the desktop date/time.
- Timezone offset: signed hour offset applied to RTC time for display.

## Networking

- NIC: network interface card.
- MAC: 48-bit layer-2 address.
- ARP: IPv4-to-MAC resolution protocol.
- DHCP: dynamic IP configuration protocol.
- ICMP: protocol used by `PING`.
- UDP: connectionless transport.
- TCP: connection-oriented transport.
- DNS: name-to-IP resolution.
- HTTP: application protocol used by `HTTPGET`.
- PCI BAR: base address register for device I/O mapping.
