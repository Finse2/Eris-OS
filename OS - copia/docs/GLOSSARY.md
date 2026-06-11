# ErisOS Glossary

> Definitions for all OS development terms used in this project.
>
> Back to [README](../README.md)

---

## Hardware & Architecture Terms

| Term | Definition |
|------|------------|
| **BIOS** | Basic Input/Output System - Firmware stored in ROM that initializes hardware and loads the bootloader when you turn on the computer |
| **CPU** | Central Processing Unit - The "brain" of the computer that executes instructions |
| **Register** | A tiny, ultra-fast storage location inside the CPU (e.g., EAX, EBX). Used for calculations and temporary data |
| **x86** | Intel's processor architecture family. ErisOS targets 32-bit x86 (also called i386 or IA-32) |
| **Port** | A numbered address used to communicate with hardware devices (e.g., port 0x60 for keyboard data) |
| **IRQ** | Interrupt Request - A signal from hardware telling the CPU "I need attention!" (e.g., key pressed) |

---

## Memory & Addressing Terms

| Term | Definition |
|------|------------|
| **Address** | A number identifying a location in memory (like a street address for bytes). Written in hex: `0xB8000` |
| **Hex / Hexadecimal** | Base-16 numbering (0-9, A-F). Prefix `0x` indicates hex. Example: `0xFF` = 255 decimal |
| **Byte** | 8 bits of data. Can hold values 0-255 or one ASCII character |
| **Word** | 2 bytes (16 bits). Can hold values 0-65535 |
| **Double Word (DWORD)** | 4 bytes (32 bits). Size of registers in 32-bit mode |
| **Stack** | A region of memory that grows/shrinks as functions call each other. Used for local variables and return addresses |
| **Pointer** | A variable that holds a memory address (points to data elsewhere) |

---

## Processor Modes

| Term | Definition |
|------|------------|
| **Real Mode** | The CPU's startup mode. 16-bit, can only access 1 MB of memory, uses segmented addressing |
| **Protected Mode** | 32-bit mode with memory protection. Full access to 4 GB address space, flat memory model |
| **Flat Memory Model** | All memory accessible through a single, continuous address space (no segments) |

---

## Boot Process Terms

| Term | Definition |
|------|------------|
| **Bootloader** | Small program (512 bytes max) that BIOS loads first. It then loads the OS kernel |
| **Boot Sector** | The first 512 bytes of a disk. Must end with magic bytes `0xAA55` to be bootable |
| **Kernel** | The core of the operating system. Manages hardware, memory, and provides services to programs |
| **GDT** | Global Descriptor Table - Defines memory segments for protected mode. Required to switch from real to protected mode |
| **IDT** | Interrupt Descriptor Table - Maps interrupt numbers to handler functions |
| **PIC** | Programmable Interrupt Controller - Routes hardware interrupts to the CPU |

---

## Assembly Language Terms

| Term | Definition |
|------|------------|
| **Assembly** | Low-level programming language using mnemonics (MOV, ADD) that translate directly to machine code |
| **Instruction** | A single operation the CPU can perform (e.g., `MOV EAX, 5` = put 5 into EAX) |
| **Operand** | The data an instruction operates on. Can be registers, memory, or immediate values |
| **Immediate** | A constant value written directly in code (e.g., the `5` in `MOV EAX, 5`) |
| **Label** | A name marking a location in code (e.g., `start:`) that you can jump to or reference |
| **Directive** | Assembler commands that don't become CPU instructions (e.g., `DB` to define data) |
| **Opcode** | The binary encoding of an instruction that the CPU actually executes |

---

## Filesystem Terms

| Term | Definition |
|------|------------|
| **Sector** | The smallest unit of disk storage - 512 bytes on most disks |
| **FAT** | File Allocation Table - A data structure tracking which sectors belong to which files |
| **LBA** | Logical Block Addressing - Numbering sectors 0, 1, 2... instead of using cylinder/head/sector |
| **CHS** | Cylinder/Head/Sector - The physical addressing scheme used by floppy disks and BIOS `int 0x13` |
| **DMA Boundary** | A 64 KB alignment boundary (`0x10000`, `0x20000`, etc.) that BIOS disk reads cannot cross |

---

## Networking Terms

| Term | Definition |
|------|------------|
| **NIC** | Network Interface Card - Hardware that connects the system to a network. ErisOS uses an RTL8029/NE2000-compatible NIC in QEMU |
| **Ethernet Frame** | The basic Layer 2 packet format on Ethernet. Contains destination MAC, source MAC, EtherType, and payload |
| **EtherType** | A 16-bit field in Ethernet frames identifying the payload protocol (e.g., `0x0800` for IPv4, `0x0806` for ARP) |
| **MAC Address** | A 48-bit hardware address used at Ethernet layer (e.g., `52:54:00:12:34:56`) |
| **Broadcast** | A frame sent to all hosts on the local network segment (destination MAC `FF:FF:FF:FF:FF:FF`) |
| **ARP** | Address Resolution Protocol - Resolves an IPv4 address to a MAC address on the local network |
| **ARP Cache** | A small table of recently learned IPv4-to-MAC mappings used to avoid repeated ARP requests |
| **DHCP** | Dynamic Host Configuration Protocol - Automatically assigns network settings like IP address, subnet mask, gateway, and DNS |
| **ICMP** | Internet Control Message Protocol - Used for control and diagnostics such as Echo Request/Reply (`ping`) |
| **IPv4** | Internet Protocol version 4 - Layer 3 protocol used for addressing and routing packets between hosts |
| **UDP** | User Datagram Protocol - Lightweight Layer 4 protocol for sending datagrams without connection setup |
| **TCP** | Transmission Control Protocol - Reliable, connection-oriented Layer 4 protocol. ErisOS currently has client-side stateful support (`TCPCONN`/`TCPSEND`/`TCPRECV`/`TCPCLOSE`/`TCPSTAT`/`TCPGET`) |
| **HTTP** | Hypertext Transfer Protocol - Application-layer protocol used by `HTTPGET` to request web resources over TCP |
| **Port (Network Port)** | A logical endpoint number inside TCP/UDP used to deliver data to the correct application (e.g., UDP port `8080`) |
| **Datagram** | A self-contained packet sent over connectionless protocols like UDP |
| **RX / TX** | Receive (RX) and Transmit (TX) data paths in the network driver |
| **Polling** | Checking device state in a loop instead of waiting for hardware interrupts |
| **Checksum** | A computed value used to detect corruption in protocol headers/payloads (IPv4 header checksum is currently validated) |
| **Subnet Mask** | A value that separates the network portion of an IP address from the host portion (e.g., `255.255.255.0`) |
| **Gateway** | The router IP used to reach addresses outside the local subnet |
| **DNS** | Domain Name System - Resolves hostnames (like `example.com`) to IP addresses |
| **Lease (DHCP Lease)** | The time period a DHCP-assigned IP configuration remains valid before renewal is needed |
| **Ping** | A connectivity test that sends ICMP Echo Request packets and measures round-trip time from Echo Replies |
| **PCI** | Peripheral Component Interconnect - Bus standard used to detect/configure devices like the RTL8029 NIC |
| **BAR (Base Address Register)** | PCI register that tells software where a device's I/O or memory-mapped registers are located |
| **I/O Base** | Starting port address of a hardware device register block (used by the NE2000 driver for register access) |

---

## Programming Terms

| Term | Definition |
|------|------------|
| **Binary** | Executable machine code that the CPU can run directly (no compilation needed at runtime) |
| **Calling Convention** | Rules for how functions receive arguments and return values. ErisOS uses cdecl |
| **cdecl** | C Declaration - Caller pushes arguments right-to-left, caller cleans stack, return value in EAX |
| **API** | Application Programming Interface - A set of functions that programs can call to use OS features |
| **Freestanding** | Code that doesn't depend on any operating system or standard library |

---

## Common Abbreviations

| Abbreviation | Meaning |
|--------------|---------|
| **ASM** | Assembly language |
| **VGA** | Video Graphics Array - Standard display hardware |
| **PIT** | Programmable Interval Timer - Generates timing signals and speaker tones |
| **ATA** | AT Attachment - Standard interface for connecting hard drives |
| **PIO** | Programmed Input/Output - CPU directly reads/writes data (vs. DMA) |
| **ELF** | Executable and Linkable Format - Standard binary format (converted to raw for ErisOS) |
| **NIC** | Network Interface Card |
| **MAC** | Media Access Control (usually refers to a 48-bit hardware address) |
| **ARP** | Address Resolution Protocol |
| **DHCP** | Dynamic Host Configuration Protocol |
| **ICMP** | Internet Control Message Protocol |
| **DNS** | Domain Name System |
| **HTTP** | Hypertext Transfer Protocol |
| **UDP** | User Datagram Protocol |
| **TCP** | Transmission Control Protocol |
| **PCI** | Peripheral Component Interconnect |
