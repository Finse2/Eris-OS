# ErisOS Internal Documentation

> Architecture, boot process, memory layout, hardware interaction, build system, and troubleshooting.
>
> Back to [README](../README.md)

---

## Understanding the Boot Process

The boot process is the journey from power-on to a running operating system. ErisOS goes through several stages, each building on the previous one.

### Stage 1: BIOS Handoff

When you turn on a computer, the CPU starts executing code from a fixed location in ROM - the BIOS (Basic Input/Output System).

The BIOS performs these steps:
1. **Power-On Self Test (POST)** - Checks that hardware is working
2. **Device initialization** - Sets up basic hardware
3. **Boot device search** - Looks for something to boot from
4. **Boot sector load** - Reads the first 512 bytes from the boot device into memory at address `0x7C00`
5. **Jump to boot code** - Transfers control to the bootloader

### Stage 2: The Bootloader (Boot.asm)

The bootloader is our code that the BIOS loads. It's limited to 512 bytes and must end with the magic number `0xAA55` to be recognized as bootable.

**What the bootloader does:**

1. **Saves the boot drive number** - The BIOS tells us which drive we booted from (in the DL register)

2. **Prints debug markers** - Letters A, B, 1, 2, 3, 4, 5, 6, 7, 8, C appear on screen to show boot progress

3. **Loads the kernel** - Uses BIOS interrupt `0x13` to read `SECTOR_COUNT` sectors from the floppy disk into memory starting at `0x10000`. The first 143 sectors are loaded by the base read sequence; extra reads are conditionally added if the kernel grows.

4. **Sets up the GDT** - The Global Descriptor Table defines memory segments for protected mode

5. **Switches to protected mode** - Enables 32-bit mode by setting the PE (Protection Enable) bit in CR0

6. **Jumps to the kernel** - Transfers control to the kernel at `0x10000`

**Understanding the debug markers:**

| Marker | Meaning |
|--------|---------|
| A      | Bootloader started |
| B      | Data segment set up |
| 1-8    | Each sector group loaded successfully |
| C      | All sectors loaded, switching to protected mode |
| D      | Protected mode entered successfully |
| X      | Disk read error (boot failed) |

### Stage 3: Kernel Entry (Kernel_Entry.asm)

The kernel entry point is the first code that runs in 32-bit protected mode. It's a small assembly stub that:

1. **Writes 'K' to the screen** - A visual marker that we reached the kernel
2. **Calls the C kernel** - The protected-mode stack is already set by `Boot.asm` (`ESP = 0x90000`)
3. **Jumps to `start()` in C** - Transfers control to the main kernel initialization

### Stage 4: Kernel Initialization (main.c)

The C kernel takes over and sets up the operating system:

1. **Initializes VGA** - Sets up the text mode display pointer (`0xB8000`)
2. **Clears the screen** - Fills the display with the default color
3. **Shows loading animation** - A brief visual indicator
4. **Initializes the filesystem** - Prepares the disk for file operations
5. **Sets up IDT/PIC/mouse IRQ path** - Interrupt descriptors + PIC masks + mouse init
6. **Enables paging** - Identity-maps the first 4 MB
7. **Initializes heap, PIT, and network** - Memory allocator, timer ticks, NIC probe/init + DHCP attempt
8. **Prints shell prompt and idles in `hlt` loop** - Shell is interrupt-driven by keyboard IRQs

---

## Sector Map - What Lives Where

ErisOS uses two separate disk images. This section maps out exactly which sectors contain what data.

### Floppy Disk Image (`bin/os-image` - 1.44 MB)

The floppy holds the boot sector and the kernel. The BIOS uses **CHS (Cylinder/Head/Sector)** addressing. Each track has **18 sectors** of 512 bytes. Two heads per cylinder gives 36 sectors (18 KB) per cylinder.

#### Floppy Geometry

| Parameter | Value |
|-----------|-------|
| Sector size | 512 bytes |
| Sectors per track | 18 |
| Heads per cylinder | 2 (Head 0, Head 1) |
| Total size | 1.44 MB (2880 sectors) |

#### Floppy Sector Layout

| CHS Address | Sector Range (LBA) | Physical Address | Size | Contents |
|-------------|-------------------|-----------------|------|----------|
| Cyl 0, Head 0, Sec 1 | 0 | `0x7C00` (BIOS loads here) | 512 B | **Boot sector** (`Boot.asm`). Bootloader code, GDT, `0xAA55` |
| Cyl 0, Head 0, Sec 2-18 | 1-17 | `0x10000`-`0x12200` | 8.5 KB (17 sec) | **Kernel start** (`.text` begins) |
| Cyl 0, Head 1, Sec 1-18 | 18-35 | `0x12200`-`0x14600` | 9 KB (18 sec) | **Kernel continued** |
| Cyl 1, Head 0, Sec 1-18 | 36-53 | `0x14600`-`0x16A00` | 9 KB (18 sec) | **Kernel continued** |
| Cyl 1, Head 1, Sec 1-18 | 54-71 | `0x16A00`-`0x18E00` | 9 KB (18 sec) | **Kernel continued** |
| Cyl 2, Head 0, Sec 1-18 | 72-89 | `0x18E00`-`0x1B200` | 9 KB (18 sec) | **Kernel continued** |
| Cyl 2, Head 1, Sec 1-18 | 90-107 | `0x1B200`-`0x1D600` | 9 KB (18 sec) | **Kernel continued** |
| Cyl 3, Head 0, Sec 1-18 | 108-125 | `0x1D600`-`0x1FA00` | 9 KB (18 sec) | **Kernel continued** |
| Cyl 3, Head 1, Sec 1-3 | 126-128 | `0x1FA00`-`0x20000` | 1.5 KB (3 sec) | **Kernel** (up to 64 KB DMA boundary) |
| Cyl 3, Head 1, Sec 4-18 | 129-143 | `0x20000`-`0x21E00` | 7.5 KB (15 sec) | **Kernel base load complete** |
| Cyl 4, Head 0, Sec 1-18 | 144-161 | `0x21E00`-`0x24200` | up to 9 KB | **Optional extra kernel sectors** |
| Cyl 4, Head 1, Sec 1-18 | 162-179 | `0x24200`-`0x26600` | up to 9 KB | **Optional extra kernel sectors** |
| Cyl 5, Head 0, Sec 1-18 | 180-197 | `0x26600`-`0x28A00` | up to 9 KB | **Optional extra kernel sectors** |
| Cyl 5+ | 198-2879 | -- | ~1.34 MB | **Unused** (padded to 1.44 MB) |

#### Bootloader Read Operations

The bootloader loads the kernel via BIOS `int 0x13`:
- **9 fixed reads** for the first 143 sectors.
- **Up to 3 optional reads** (`EXTRA1`/`EXTRA2`/`EXTRA3`) for sectors 144-197.

| Read # | Marker | Cylinder | Head | Sectors | Count | Running Total | Destination |
|--------|--------|----------|------|---------|-------|---------------|-------------|
| 1 | `1` | 0 | 0 | 2-18 | 17 | 17 | `0x10000` |
| 2 | `2` | 0 | 1 | 1-18 | 18 | 35 | `0x12200` |
| 3 | `3` | 1 | 0 | 1-18 | 18 | 53 | `0x14600` |
| 4 | `4` | 1 | 1 | 1-18 | 18 | 71 | `0x16A00` |
| 5 | `5` | 2 | 0 | 1-18 | 18 | 89 | `0x18E00` |
| 6 | `6` | 2 | 1 | 1-18 | 18 | 107 | `0x1B200` |
| 7 | `7` | 3 | 0 | 1-18 | 18 | 125 | `0x1D600` |
| 8 | -- | 3 | 1 | 1-3 | 3 | 128 | `0x1FA00` |
| 9 | `8` | 3 | 1 | 4-18 | 15 | **143** | `0x20000` |
| 10 (optional) | -- | 4 | 0 | 1-18 | up to 18 | up to 161 | `0x21E00` |
| 11 (optional) | -- | 4 | 1 | 1-18 | up to 18 | up to 179 | next |
| 12 (optional) | -- | 5 | 0 | 1-18 | up to 18 | up to **197** | next |

**Why reads 8 and 9 are split:** BIOS `int 0x13` cannot read across a **64 KB DMA boundary** (`0x10000`, `0x20000`, etc.). After read 7, the next address is `0x1FA00` -- only 3 sectors from `0x20000`. The read is split: 3 sectors to fill to the boundary, then ES is adjusted to `0x2000` before reading the remaining 15 sectors.

#### Kernel Binary Layout (in memory at `0x10000`)

| Memory Range | Linker Section | Contents |
|-------------|----------------|----------|
| `0x10000`-... | `.text` | Executable code: entry point, kernel functions, parser, editor, assembler, IDT, ATA |
| ...-... | `.rodata` | Read-only data: string literals, help text, scancode tables |
| ...-... | `.data` | Initialized global variables |
| ...-... | `.bss` | Uninitialized globals: buffers, FAT, directory table, aligned paging structures |

### Hard Disk Image (`bin/hdd.img` - 10 MB)

The hard disk uses **LBA (Logical Block Addressing)** via ATA PIO ports (`0x1F0`-`0x1F7`). The filesystem lives here, completely separate from the boot floppy.

#### Hard Disk Sector Layout

| LBA Sector | Size | Contents |
|------------|------|----------|
| **0** | 512 B | **Protected sector** -- Write-protected in `ata.asm`. Used by `PUT`/`GET` test commands. |
| **1** | 512 B | **FAT (File Allocation Table)** -- 128 entries x 4 bytes. Tracks sector chains for files. |
| **2** | 512 B | **Directory Table** -- 16 entries x 32 bytes. Stores filenames, sizes, start sectors, flags. |
| **3-130** | 64 KB | **Data sectors** -- Where actual file content is stored, allocated on demand via FAT. |
| **131-20479** | ~9.9 MB | **Unused** -- Available for future expansion. |

#### FAT Entry Values

| Value | Meaning |
|-------|---------|
| `0x00000000` | Free sector |
| `0xFFFFFFFF` | End of file chain |
| `0xFFFFFFFE` | Bad sector (reserved) |
| `3`-`130` | Pointer to next sector in chain |

#### Directory Entry Structure (32 bytes each)

```
Offset  Size    Field
------  ------  ------------------
0x00    12 B    Filename (null-padded)
0x0C    4 B     File size (bytes)
0x10    4 B     Start sector (LBA)
0x14    1 B     Flags: 0x01=Used, 0x02=Executable
0x15    11 B    Reserved (padding)
```

### Summary: All Sectors at a Glance

```
FLOPPY DISK (bin/os-image) - CHS addressing via BIOS int 0x13
+----------------------------------------------------------+
| Sector 0          | Boot sector (512 B)                  |
| Sectors 1-SECTOR_COUNT | Kernel binary (dynamic size)     |
| Sectors >SECTOR_COUNT  | Unused (padded to 1.44 MB)       |
+----------------------------------------------------------+

HARD DISK (bin/hdd.img) - LBA addressing via ATA PIO
+----------------------------------------------------------+
| Sector 0          | Protected (test data / PUT/GET)      |
| Sector 1          | FAT - File Allocation Table          |
| Sector 2          | Directory Table (16 file entries)    |
| Sectors 3-130     | File data (128 usable sectors)       |
| Sectors 131-20479 | Unused (future expansion)            |
+----------------------------------------------------------+
```

---

## Memory Organization

### Why Memory Layout Matters

In protected mode, we have access to a flat 4GB address space (theoretically). But certain addresses are special:

- Some addresses map to hardware (like the VGA display)
- The BIOS leaves important data in certain locations
- We need to avoid overwriting ourselves!

### ErisOS Memory Map

```
Address Range          Size      Purpose
-------------------------------------------------------
0x00000 - 0x003FF      1 KB      Real Mode Interrupt Vector Table
                                 (Not used after boot)

0x00400 - 0x004FF      256 B     BIOS Data Area
                                 (Contains hardware info)

0x00500 - 0x07BFF      ~30 KB    Free memory
                                 (Bootloader uses temporarily)

0x07C00 - 0x07DFF      512 B     Boot Sector
                                 (Where BIOS loads us)

0x07E00 - 0x0FFFF      ~33 KB    Free memory

0x10000 - ~0x3FFFF     Variable  Kernel image (.text/.rodata/.data/.bss)
                                 (Exact end depends on build size)

~kernel_end - 0x4FFFF  Variable  Free memory
                                 (Can shrink/grow as kernel changes)

0x50000 - 0x5FFFF      64 KB     User Program Area
                                 (Where EXEC loads programs)

0x60000 - 0x8FFFF      192 KB    Kernel Heap
                                 (Dynamic memory allocation)

0x90000 - 0x9FFFF      64 KB     Stack
                                 (Grows downward from 0x90000)

0xA0000 - 0xBFFFF      128 KB    VGA Memory
                                 (Video display buffer)

0xB8000 - 0xB8F9F      4000 B    VGA Text Mode Buffer
                                 (80 x 25 x 2 bytes)

0xC0000 - 0xFFFFF      256 KB    ROM / Reserved
                                 (BIOS, video BIOS, etc.)
```

### Full 32-bit Address Space Visualization (`0x00000000` to `0xFFFFFFFF`)

The map below shows where the key boot/runtime regions sit inside the full 4 GB x86 address space.

```
High Addresses
0xFFFFFFFF +--------------------------------------------------+
           | Unmapped / not used by current ErisOS build      |
           | (future user spaces, kernel virtual mappings)    |
           |                                                  |
0x00400000 +--------------------------------------------------+
           | End of current identity-mapped region (4 MB)     |
           |                                                  |
           | Free / future expansion                          |
0x000C0000 +--------------------------------------------------+
           | ROM / Reserved (BIOS, option ROMs, video BIOS)   |
0x000B8000 +--------------------------------------------------+
           | VGA text buffer (80x25x2)                        |
0x000A0000 +--------------------------------------------------+
           | VGA memory aperture                              |
0x00090000 +--------------------------------------------------+
           | Stack (grows downward)                           |
0x00060000 +--------------------------------------------------+
           | Kernel heap (kmalloc/kfree)                      |
0x00050000 +--------------------------------------------------+
           | User program load area (EXEC)                    |
0x00010000 +--------------------------------------------------+
           | Kernel image (.text/.rodata/.data/.bss)          |
0x00007C00 +--------------------------------------------------+
           | Bootloader (BIOS loads sector 0 here)            |
0x00000400 +--------------------------------------------------+
           | BIOS Data Area                                   |
0x00000000 +--------------------------------------------------+
           | IVT (real-mode interrupt vectors)                |
Low Addresses
```

Notes:
- CPU starts in real mode and executes BIOS firmware before control reaches `0x7C00`.
- ErisOS currently identity-maps the first `4 MB`; addresses above that are not mapped by default.
- The practical "hot" ranges for day-to-day kernel work are `0x10000` (kernel), `0x50000` (programs), `0x60000` (heap), and `0x90000` (stack).

### Key Memory Addresses

| Address | Name | Description |
|---------|------|-------------|
| `0x7C00` | Boot sector | Where BIOS loads the bootloader |
| `0x10000` | Kernel base | Where the kernel is loaded |
| dynamic (aligned) | paging_dir | 4 KB-aligned page directory in kernel `.bss` |
| dynamic (aligned) | paging_table0 | 4 KB-aligned first page table in kernel `.bss` |
| `0x50000` | Program load | Where user programs are loaded |
| `0x60000` | Heap start | Dynamic memory allocation (192 KB) |
| `0x90000` | Stack top | Stack pointer starts here |
| `0xB8000` | VGA text | Write here to display text |

### How VGA Text Memory Works

The VGA text mode buffer at `0xB8000` is organized as a grid of character cells:

```
Each cell = 2 bytes:
  Byte 0: ASCII character code
  Byte 1: Color attribute

Color attribute format:
  Bits 0-3: Foreground color (0-15)
  Bits 4-6: Background color (0-7)
  Bit 7:    Blink enable
```

To write "Hi" in white on blue at the top-left corner:

```c
char* video = (char*)0xB8000;
video[0] = 'H';      // Character
video[1] = 0x1F;     // White (F) on Blue (1)
video[2] = 'i';      // Character
video[3] = 0x1F;     // White (F) on Blue (1)
```

---

## Memory Paging

### What is Paging?

Paging is a CPU feature that translates **virtual addresses** (what code uses) into **physical addresses** (where data actually lives in RAM). The CPU uses in-memory data structures -- a **page directory** and **page tables** -- to perform this translation automatically on every memory access.

Paging provides the foundation for:
- **Memory protection** -- preventing programs from accessing kernel memory
- **Process isolation** -- giving each program its own virtual address space
- **Demand paging** -- loading memory on the fly from disk

### ErisOS Paging: Identity Mapping

ErisOS uses the simplest form of paging: **identity mapping**, where every virtual address equals the corresponding physical address. This means all existing code (kernel, user programs, VGA writes) works without any changes, but paging is now active and can be extended in the future.

The first **4 MB** of memory is identity-mapped, covering everything ErisOS needs:

| Region | Address Range | What It Contains |
|--------|---------------|-----------------|
| Low memory | `0x00000`-`0x0FFFF` | IVT, BIOS data, free memory |
| Kernel | `0x10000`-`~0x3FFFF` | Kernel code/data + aligned paging arrays |
| Free | `~kernel_end`-`0x4FFFF` | Available memory (varies by build) |
| User programs | `0x50000`-`0x5FFFF` | Where EXEC loads programs |
| Stack | `0x90000`-`0x9FFFF` | Stack (grows downward) |
| VGA | `0xB8000`-`0xB8F9F` | Text mode display buffer |

### How It Works

The x86 paging mechanism uses a two-level table structure:

```
Virtual Address: 0x000B8000 (VGA text buffer)

Split into:
  [Directory Index: 10 bits] [Table Index: 10 bits] [Offset: 12 bits]
  [    0x000 (0)           ] [    0x0B8 (184)     ] [  0x000 (0)    ]

Lookup:
  1. CR3 points to `paging_dir` (runtime physical address)
  2. Entry 0 of page directory -> `paging_table0` (runtime physical address)
  3. Entry 184 of page table -> Physical page 0xB8000
  4. Add offset 0x000 -> Physical address 0xB8000

Result: Virtual 0xB8000 = Physical 0xB8000 (identity mapped)
```

### Page Table Entry Format

Each page table entry is 4 bytes (32 bits):

```
Bits 31-12: Physical page address (aligned to 4 KB)
Bits 11-9:  Available for OS use
Bit 6:      Dirty (page has been written to)
Bit 5:      Accessed (page has been read)
Bit 2:      User/Supervisor (0 = kernel only, 1 = user accessible)
Bit 1:      Read/Write (0 = read only, 1 = writable)
Bit 0:      Present (0 = not in memory, 1 = valid page)
```

ErisOS sets each entry to `(physical_addr | 0x03)` -- present and writable.

### Memory Layout of Page Structures

The page directory and first page table are allocated as 4 KB-aligned kernel globals:
- `paging_dir[1024]`
- `paging_table0[1024]`

Their exact physical addresses are build-dependent, but both live in kernel `.bss`. `PGINFO` prints the live addresses currently loaded in CR3 and PDE[0].

### Page Fault Handler

If the CPU accesses a virtual address that is not mapped (e.g., above 4 MB), it triggers **exception 14 (Page Fault)**. ErisOS installs a handler that:

1. Reads the **CR2** register (holds the faulting virtual address)
2. Prints a diagnostic message with the address
3. Halts the system (the fault is unrecoverable)

### Viewing Paging Status

Use the `PGINFO` command to display:
- Whether paging is enabled
- Current CR0 and CR3 register values
- Page directory and page table addresses
- Sample page table entries for key memory regions

### Future Extensions

With paging active, the following features become possible:
- **User/kernel isolation** -- Map user programs with USER bit set, kernel pages without
- **On-demand allocation** -- Mark pages as not-present and allocate on page fault
- **Process address spaces** -- Give each EXEC program its own page directory
- **Memory-mapped I/O** -- Map device memory into specific virtual ranges

---

## Dynamic Memory Allocation

ErisOS provides a simple **first-fit free-list heap allocator** so the kernel (and user programs) can dynamically allocate and free memory at runtime.

### What is the Heap?

The heap is a dedicated region of RAM reserved for dynamic allocations -- memory requested at runtime with `kmalloc()` and returned with `kfree()`. Unlike the stack (which grows/shrinks automatically with function calls), the heap is explicitly managed by the programmer.

### Heap Location

```
0x60000 - 0x8FFFF    192 KB    Kernel Heap
```

This region is identity-mapped by paging (within the first 4 MB), safely above user programs (`0x50000`) and below the stack (`0x90000`).

### Block Structure

Every allocation is preceded by an 8-byte **block header**:

```c
struct heap_block {
    unsigned int size;   // Size of usable data (excludes this header)
    unsigned int used;   // 0 = free, 1 = allocated
};
```

The allocator walks the heap block-by-block. To find the next block from the current one:

```
next = (char*)current + sizeof(header) + current->size
```

### How kmalloc() Works

1. Walk the free list from the beginning of the heap
2. Find the **first free block** whose `size >= requested_size` (first-fit)
3. If the block is much larger than needed, **split** it into two blocks: one for the allocation and one new free block for the remainder
4. Mark the block as `used = 1`
5. Return a pointer to the memory just after the header

All allocations are aligned to 4 bytes for proper 32-bit alignment.

### How kfree() Works

1. Find the block header (it sits immediately before the pointer that was returned by `kmalloc`)
2. Mark the block as `used = 0`
3. **Coalesce**: walk the entire block list and merge any adjacent free blocks into one larger free block. This prevents fragmentation.

### API

| Function | Signature | Description |
|----------|-----------|-------------|
| `kmalloc` | `void* kmalloc(unsigned int size)` | Allocate `size` bytes. Returns pointer to usable memory, or `0` (NULL) on failure |
| `kfree` | `void kfree(void* ptr)` | Free a previously allocated block. Coalesces adjacent free blocks |

These functions are available to user programs via the KernelAPI at offsets +40 (`kmalloc`) and +44 (`kfree`).

### Initialization

`heap_init()` is called during boot, right after paging is enabled. It creates a single free block spanning the entire 192 KB heap region (minus 8 bytes for the initial header).

### Monitoring

Use the `HEAP` shell command to see:
- Total heap size, used/free bytes
- Number of allocated and free blocks
- Largest free block available
- Header overhead
- A block-by-block listing (first 8 blocks)

### Limitations and Future Extensions

- **No per-process heaps** -- all allocations share a single global heap
- **First-fit only** -- can lead to external fragmentation over time; a best-fit or buddy allocator could improve this
- **No guard pages** -- buffer overflows in one allocation can corrupt the next block's header
- Future extensions could add size-class caches (slab allocator) or per-program heaps

---

## Hardware Interaction

ErisOS directly interfaces with hardware through I/O ports and memory-mapped registers.

### VGA Text Mode

**Memory address:** `0xB8000`

The VGA controller provides an 80x25 character text display. Each character cell uses 2 bytes:

```
Byte 0: Character (ASCII code)
Byte 1: Attribute (color)

Attribute byte:
  Bit 7:    Blink
  Bits 4-6: Background color (0-7)
  Bits 0-3: Foreground color (0-15)
```

**Color values:**

| Value | Color | Value | Color |
|-------|-------|-------|-------|
| 0 | Black | 8 | Dark Gray |
| 1 | Blue | 9 | Light Blue |
| 2 | Green | 10 | Light Green |
| 3 | Cyan | 11 | Light Cyan |
| 4 | Red | 12 | Light Red |
| 5 | Magenta | 13 | Pink |
| 6 | Brown | 14 | Yellow |
| 7 | Light Gray | 15 | White |

### Keyboard Input

**Ports:**
- `0x60` - Data port (read scancodes)
- `0x64` - Status/command port

**Interrupt:** IRQ1 -> Interrupt 0x21

When a key is pressed:
1. The keyboard controller generates IRQ1
2. Our handler reads the scancode from port 0x60
3. The scancode is translated to ASCII
4. The character is added to the input buffer

**Scancode types:**
- **Make codes** - Key pressed (0x01-0x58)
- **Break codes** - Key released (make code + 0x80)
- **Extended codes** - Prefixed with 0xE0 (arrow keys, etc.)

### Mouse Input

**Ports:**
- `0x60` - Data port (shared with keyboard)
- `0x64` - Status/command port

**Interrupt:** IRQ12 -> Interrupt 0x2C

The PS/2 mouse sends 3-byte packets:

```
Byte 0: Status
  Bit 0: Left button
  Bit 1: Right button
  Bit 2: Middle button
  Bit 4: X sign bit
  Bit 5: Y sign bit

Byte 1: X movement (signed)
Byte 2: Y movement (signed)
```

### PC Speaker

**Ports:**
- `0x42` - PIT Channel 2 data
- `0x43` - PIT mode/command
- `0x61` - System control (speaker enable)

**To play a tone:**

1. Calculate the divisor: `1193180 / frequency`
2. Set PIT channel 2 to square wave mode
3. Load the divisor
4. Enable the speaker via port 0x61

**To stop sound:**

1. Disable the speaker via port 0x61

### ATA Disk Controller

**Ports:** `0x1F0` - `0x1F7`

| Port | Read | Write |
|------|------|-------|
| 0x1F0 | Data | Data |
| 0x1F1 | Error | Features |
| 0x1F2 | Sector Count | Sector Count |
| 0x1F3 | LBA Low | LBA Low |
| 0x1F4 | LBA Mid | LBA Mid |
| 0x1F5 | LBA High | LBA High |
| 0x1F6 | Drive/Head | Drive/Head |
| 0x1F7 | Status | Command |

**Commands:**
- `0x20` - Read sectors
- `0x30` - Write sectors

**PIO transfer:**
1. Wait for drive ready
2. Select drive and set LBA
3. Send command
4. Wait for data ready
5. Read/write 256 words (512 bytes)

### RTL8029 / NE2000 Network Controller

ErisOS includes a basic NE2000-compatible driver for the **RTL8029 PCI NIC** (`vendor 0x10EC`, `device 0x8029`).

Current support:
- PCI scan via config ports `0xCF8` / `0xCFC`
- BAR0 I/O base detection
- NIC reset + ring buffer setup
- Raw Ethernet transmit (`NETTX`)
- IRQ11-driven RX completion + poll/dequeue receive (`NETRX`) with TX completion polling
- ARP request/reply handling
- ARP cache updates from ARP traffic
- Minimal IPv4 framing for transmit (with subnet/gateway next-hop resolution)
- IPv4 header parsing/checksum validation on receive
- DHCPv4 client (boot-time attempt + manual renew)
- ICMP echo request/reply (`PING`)
- UDP header parsing + fixed queue for bound shell ports
- DNS A-record resolver over UDP (`DNS`)
- Client-side TCP state machine (connect/send/recv/close/stat)
- HTTP/1.0 client command over TCP (`HTTPGET`)

Detailed networking internals (driver flow, ring layout, command frame format, RX behavior) are documented in [NETWORKING.md](NETWORKING.md).

Shell commands:
- `NETINFO` - Show NIC I/O base, IRQ line, and MAC address
- `NETTX` - Send a broadcast raw Ethernet test frame
- `NETRX` - Poll once and dump the first bytes of a received frame
- `ARPWHO <ip>` - Send ARP request and resolve IPv4 to MAC (with polling timeout)
- `ARPCACHE` - Show learned ARP cache entries
- `IPTX <ip>` - Send a minimal IPv4 test packet (EtherType 0x0800, proto 0xFD)
- `DHCP` - Request/renew IPv4 settings via DHCP
- `PING <ip>` - Send ICMP echo request and wait for echo reply
- `UDPBIND <port>` - Bind a UDP local port
- `UDPSEND <src_port> <dst_ip> <dst_port> <message>` - Send UDP datagram
- `UDPRECV <port>` - Receive one queued UDP datagram for port
- `UDPSOCK` - List bound UDP ports
- `TCPCONN <ip> <port>` - Open TCP client connection
- `TCPSEND <text>` - Send payload on established TCP connection
- `TCPRECV` - Print/clear buffered TCP receive data
- `TCPCLOSE` - Gracefully close TCP connection
- `TCPSTAT` - Show TCP state/counters
- `DNS <hostname> [dns_server_ip]` - Resolve hostname to IPv4 address (A record)
- `TCPGET <ip> <port> [path]` - Minimal TCP client utility (connect + GET + print response)
- `HTTPGET <host_or_ip> [path] [port]` - HTTP client utility with optional DNS resolution

This is intentionally early-stage networking. ARP, DHCPv4, ICMP ping, minimal IPv4, UDP shell APIs, and a client-side TCP state machine (with retransmit/close handling) are implemented.

### Programmable Interrupt Controller (PIC)

**Ports:**
- `0x20`, `0x21` - Master PIC (IRQ 0-7)
- `0xA0`, `0xA1` - Slave PIC (IRQ 8-15)

The PIC must be remapped because default IRQ 0-7 conflict with CPU exceptions:

| IRQ | Default | Remapped | Device |
|-----|---------|----------|--------|
| 0 | 0x08 | 0x20 | Timer |
| 1 | 0x09 | 0x21 | Keyboard |
| 12 | 0x74 | 0x2C | Mouse |

### Centralized Hardware Port Reference

This table centralizes all primary I/O ports used by ErisOS kernel code and drivers.

| Port(s) | Device/Block | Purpose in ErisOS |
|---------|---------------|-------------------|
| `0x20` | PIC1 command | Master PIC control / EOI |
| `0x21` | PIC1 data | Master PIC mask/remap config |
| `0xA0` | PIC2 command | Slave PIC control / EOI |
| `0xA1` | PIC2 data | Slave PIC mask/remap config |
| `0x40` | PIT ch0 data | System timer divisor load (IRQ0 at 100 Hz) |
| `0x42` | PIT ch2 data | Speaker tone divisor load |
| `0x43` | PIT command | PIT mode/control register |
| `0x60` | KBC/PS2 data | Keyboard scancodes + mouse packet bytes |
| `0x64` | KBC/PS2 status/cmd | Keyboard/mouse controller command/status |
| `0x61` | System control | PC speaker gate/enable bits |
| `0x3D4` | VGA CRT index | Hardware cursor register index |
| `0x3D5` | VGA CRT data | Hardware cursor register data |
| `0x1F0` | ATA data | PIO data read/write (sector payload) |
| `0x1F1` | ATA error/features | Error read / feature write |
| `0x1F2` | ATA sector count | Number of sectors for command |
| `0x1F3` | ATA LBA low | LBA bits 0-7 |
| `0x1F4` | ATA LBA mid | LBA bits 8-15 |
| `0x1F5` | ATA LBA high | LBA bits 16-23 |
| `0x1F6` | ATA drive/head | Drive select + high LBA bits |
| `0x1F7` | ATA status/command | Status read / command write |
| `0xCF8` | PCI config address | PCI config mechanism #1 address port |
| `0xCFC` | PCI config data | PCI config mechanism #1 data port |
| `io_base + 0x00` | NE2000 `CMD` | NIC command register |
| `io_base + 0x01` | NE2000 `PSTART` / `PAR0` | RX ring start (page 0) / MAC bytes (page 1) |
| `io_base + 0x02` | NE2000 `PSTOP` | RX ring stop |
| `io_base + 0x03` | NE2000 `BNRY` | RX boundary pointer |
| `io_base + 0x04` | NE2000 `TPSR` | TX start page |
| `io_base + 0x05` | NE2000 `TBCR0` | TX byte count low |
| `io_base + 0x06` | NE2000 `TBCR1` | TX byte count high |
| `io_base + 0x07` | NE2000 `ISR` / `CURR` | Interrupt status (page 0) / RX current page (page 1) |
| `io_base + 0x08` | NE2000 `RSAR0` / `MAR0` | Remote DMA addr low / multicast filter (page 1) |
| `io_base + 0x09` | NE2000 `RSAR1` | Remote DMA addr high |
| `io_base + 0x0A` | NE2000 `RBCR0` | Remote DMA byte count low |
| `io_base + 0x0B` | NE2000 `RBCR1` | Remote DMA byte count high |
| `io_base + 0x0C` | NE2000 `RCR` | Receive configuration |
| `io_base + 0x0D` | NE2000 `TCR` | Transmit configuration |
| `io_base + 0x0E` | NE2000 `DCR` | Data configuration |
| `io_base + 0x0F` | NE2000 `IMR` | Interrupt mask (RX IRQ enabled, TX polled) |
| `io_base + 0x10` | NE2000 data port | Remote DMA data window |
| `io_base + 0x1F` | NE2000 reset | Hardware reset trigger |

---

## Build System

The `compile.bat` script handles the entire build process.

### Build Steps

1. **Assemble kernel modules**
   ```
   nasm Kernel_Entry.asm -f elf -o bin\Entry.bin
   nasm IDT.asm -f elf -o bin\IDT.bin
   nasm ata.asm -f elf -o bin\ata.bin
   ```

2. **Compile C code**
   ```
   gcc -m32 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -c main.c -o bin\kernel.o
   gcc -m32 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -c filesystem.c -o bin\filesystem.o
   gcc -m32 -ffreestanding -fno-stack-protector -mno-stack-arg-probe -c ne2000.c -o bin\ne2000.o
   ```

3. **Link everything**
   ```
   ld -T link.ld -o bin\Kernel.img bin\Entry.bin bin\kernel.o bin\filesystem.o bin\ne2000.o bin\IDT.bin bin\ata.bin
   ```

4. **Convert to raw binary**
   ```
   objcopy -O binary bin\Kernel.img bin\kernel.bin
   ```

5. **Calculate kernel size**
   - Count bytes in kernel.bin
   - Divide by 512, round up
   - Write to `boot_sectors.inc`

6. **Assemble bootloader**
   ```
   nasm Boot.asm -f bin -o bin\bootsect.bin
   ```

7. **Create boot image**
   ```
   copy /b bin\bootsect.bin+bin\kernel.bin bin\os-image
   ```

8. **Pad to floppy size** - Pad `os-image` to 1.44 MB

9. **Create hard disk image** - Create `bin\hdd.img` (10 MB) if it doesn't exist

10. **Launch QEMU**
    ```
    qemu-system-i386 -drive file=bin\os-image,format=raw,index=0,if=floppy -drive file=bin\hdd.img,format=raw,index=0,if=ide -netdev user,id=net0 -device ne2k_pci,netdev=net0 -boot a
    ```

### The Linker Script (link.ld)

The linker script controls memory layout:

```ld
SECTIONS
{
    . = 0x10000;        /* Kernel loads here */

    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(.bss*) *(COMMON) }
}
```

Key points:
- Code starts at `0x10000`
- `.rodata` is included (required for string literals)
- Sections are placed contiguously

### Dynamic Kernel Size

The bootloader needs to know how many sectors to load. This is computed automatically:

1. After linking, `compile.bat` checks `kernel.bin` size
2. Calculates: `sectors = ceil(size / 512)`
3. Writes `boot_sectors.inc`:
   ```asm
   %define SECTOR_COUNT <computed>
   ```
4. Bootloader includes this file and loads that many sectors

**Important:** Always use `compile.bat` to ensure the sector count stays synchronized.

Current bootloader extension reads cover up to **197 sectors** total. If `SECTOR_COUNT` exceeds 197, extend `Boot.asm` with more CHS reads (and maintain 64 KB DMA-boundary-safe splits).

### Dual-Drive Configuration

ErisOS uses two virtual drives:

| Drive | File | Size | Purpose |
|-------|------|------|---------|
| Floppy | `bin/os-image` | 1.44 MB | Boot + kernel |
| Hard disk | `bin/hdd.img` | 10 MB | Filesystem storage |

**Why two drives?**

1. The bootloader uses BIOS `int 0x13` with floppy geometry (CHS addressing)
2. The ATA driver uses hard disk I/O ports (LBA addressing)
3. Keeping them separate simplifies both drivers

---

## File Descriptions

### Source Files

| File | Type | Description |
|------|------|-------------|
| `Boot.asm` | Assembly (16-bit) | First-stage bootloader. Loaded by BIOS at 0x7C00, loads kernel from floppy, sets up GDT, switches to protected mode |
| `Kernel_Entry.asm` | Assembly (32-bit) | Kernel entry point. First code in protected mode; writes marker and calls `start()` |
| `IDT.asm` | Assembly (32-bit) | Interrupt handling. IDT, keyboard/mouse handlers, PIC remapping |
| `ata.asm` | Assembly (32-bit) | Disk driver. ATA PIO read/write sector functions |
| `ne2000.c` | C | Network driver. PCI detect for RTL8029 and NE2000-compatible RX IRQ + TX polling |
| `ne2000.h` | C Header | Network API declarations used by kernel/shell |
| `main.c` | C | Main kernel. Display functions, command loop, initialization |
| `extra.h` | C Header | Extended features. Command parser, editor, assembler, audio, colors |
| `filesystem.c` | C | Filesystem logic. FAT operations, file CRUD, directory management |
| `filesystem.h` | C Header | Filesystem API. Structures and function declarations |

### Build & Configuration Files

| File | Type | Description |
|------|------|-------------|
| `compile.bat` | Batch Script | Build automation. Assembles, compiles, links, creates images, launches QEMU |
| `link.ld` | Linker Script | Memory layout. Kernel sections placed starting at 0x10000 |
| `boot_sectors.inc` | Generated | Auto-generated `SECTOR_COUNT` for bootloader |

### Output Files (bin/ folder)

| File | Size | Description |
|------|------|-------------|
| `os-image` | 1.44 MB | Bootable floppy image (boot sector + kernel) |
| `hdd.img` | 10 MB | Hard disk image (filesystem storage) |
| `kernel.bin` | Variable | Raw kernel binary (size depends on current features) |

---

## Command History

The shell keeps a circular buffer of the last 16 commands. Pressing **Up** and **Down** arrows at the prompt navigates through previously executed commands.

### How It Works

1. When the user presses **Enter**, the command is saved to a circular buffer (`cmd_history[16][256]`) before execution. Empty commands and exact duplicates of the most recent entry are not saved.
2. Pressing **Up** (`0xE0 0x48`) navigates backward (older commands). On the first press, the in-progress input line is saved so it can be restored later.
3. Pressing **Down** (`0xE0 0x50`) navigates forward (newer commands). Pressing Down past the newest entry restores the original in-progress line.
4. The current input line is erased and replaced on screen each time the user navigates.

### Implementation Details

| Variable | Type | Purpose |
|----------|------|---------|
| `cmd_history[16][256]` | `char[][]` | Circular buffer of past commands |
| `history_count` | `int` | Total commands ever saved (modulo 16 for slot) |
| `history_browse` | `int` | Current browse index, or `-1` when not browsing |
| `cmd_saved[256]` | `char[]` | Saves the in-progress line when browsing starts |

Key helper functions in `main.c`:
- `historySave(cmd)` -- Appends a command to the circular buffer
- `shellReplaceLine(newcmd)` -- Erases the current input line on screen and replaces it with `newcmd`

Arrow keys arrive as extended scan codes: the keyboard first sends `0xE0`, which sets the `extended_scancode` flag, then the actual key code (`0x48` for Up, `0x50` for Down). The shell checks `is_extended` before the normal key handling to intercept these.

---

## Scrolling Terminal Buffer

ErisOS keeps a **scrollback buffer** of the last 100 lines that have scrolled off the top of the screen. You can scroll back through previous output using the **mouse scroll wheel**.

### How It Works

1. Every time the screen scrolls (a line leaves the top), the full 160-byte line (80 characters + 80 color attributes) is saved into a circular ring buffer.
2. **Scrolling the mouse wheel up** enters scroll view mode. The current VGA screen is snapshot-saved and the display is redrawn from the scrollback buffer + saved screen, shifted up by 3 lines per tick.
3. **Scrolling the mouse wheel down** scrolls forward. When you reach the bottom, the live screen is restored.
4. Pressing **any key** while in scroll view immediately exits scroll mode, restores the live screen, and processes the key normally.

### Mouse Scroll Wheel Support

The scroll wheel is enabled during mouse initialization using the **Intellimouse protocol**. The standard PS/2 mouse sends 3-byte packets (buttons, X delta, Y delta). The Intellimouse protocol extends this to 4-byte packets, where the 4th byte is the Z-axis (scroll wheel) delta.

To enable the scroll wheel, the driver sends a "magic sequence" during init:
1. Set sample rate to 200
2. Set sample rate to 100
3. Set sample rate to 80
4. Query device ID -- if it returns `3`, the Intellimouse protocol is active

After this, each mouse packet includes a signed byte for the scroll wheel: negative values mean scroll up, positive means scroll down.

### Data Structures

| Variable | Type | Purpose |
|----------|------|---------|
| `scrollback_buf[16000]` | `char[]` | Ring buffer: 100 lines x 160 bytes |
| `screen_backup[4000]` | `char[]` | VGA snapshot when entering scroll view |
| `scrollback_write` | `int` | Next write slot in ring buffer (0-99) |
| `scrollback_count` | `int` | Total lines stored (max 100) |
| `scroll_offset` | `int` | Lines scrolled back (0 = live view) |
| `cell_backup` | `int` | Saved CELL position for restoring |

### Visual Indicator

When in scroll view, a `SCROLL` label appears at the bottom-right corner of the screen with an inverted color scheme (black on light gray), so you know you're viewing history.

### Key Functions

- `scroll()` (modified) -- Now saves line 0 to the ring buffer before shifting lines up
- `scrollViewRedraw()` -- Composites the view from scrollback + saved screen
- `scrollPageUp()` / `scrollPageDown()` -- Navigate through history
- `scrollExitView()` -- Restores the live screen

### Limitations

- Only 100 lines of scrollback (~4 screens worth)
- The editor has its own screen management and does not use scrollback
- `CLS` resets the scroll view but does not clear the scrollback buffer

---

## PIT Timer (IRQ0)

ErisOS uses the **Programmable Interval Timer** (Intel 8253/8254) to track real time. Channel 0 of the PIT is configured to fire **IRQ0 at 100 Hz** (every 10 ms), giving the system a precise tick counter.

### How It Works

1. During boot, `pit_init()` programs PIT channel 0 in **mode 3** (square wave generator) with a divisor of 11932 (1,193,182 Hz / 100 = ~100 Hz).
2. Every 10 ms, the PIT fires IRQ0 which triggers `isr0_Handler()`.
3. The handler increments a global `pit_ticks` counter and sends EOI to the PIC.
4. The `UPTIME` command reads `pit_ticks`, divides by 100 to get seconds, and displays the result in `Hh Mm S.SSs` format.

### PIT Configuration

| Register | Port | Value | Meaning |
|----------|------|-------|---------|
| Command | `0x43` | `0x36` | Channel 0, lobyte/hibyte, mode 3 (square wave) |
| Data low | `0x40` | `0x9C` | Low byte of divisor (11932 = 0x2E9C) |
| Data high | `0x40` | `0x2E` | High byte of divisor |

### Key Variables

| Variable | Type | Location | Purpose |
|----------|------|----------|---------|
| `pit_ticks` | `volatile unsigned int` | `main.c` | Total ticks since boot (100 per second) |

### IDT/PIC Setup

- IRQ0 maps to interrupt vector `0x20` (after PIC remapping)
- ISR stub `_isr0` in `IDT.asm` saves registers, calls `isr0_Handler()`, restores, and returns
- PIC1 mask changed from `0xF9` to `0xF8` to unmask IRQ0 alongside IRQ1 and IRQ2

### Uptime Display

The `UPTIME` command computes hours/minutes/seconds from `pit_ticks`:

```
total_seconds = pit_ticks / 100
hours   = total_seconds / 3600
minutes = (total_seconds % 3600) / 60
seconds = total_seconds % 60
```

`SYSINFO` also shows a condensed uptime line.

### Future Extensions

- **Sleep/delay functions** -- `sleep(ms)` by busy-waiting on `pit_ticks`
- **Preemptive scheduling** -- Use the timer to switch between processes
- **Real-time clock** -- Combine PIT ticks with CMOS RTC for wall-clock time

---

## Troubleshooting

### Build / Link Errors

**Symptom:** `main.c: ... error: 'PIT_HZ' undeclared`
- `pit_init()` is using `PIT_HZ` before the macro is defined.
- Move `#define PIT_HZ 100` above `pit_init()` (near top-level declarations in `main.c`).

**Symptom:** `IDT.asm: ... undefined reference to 'isr0_Handler'`
- This usually means the C ISR handler symbol was not emitted because compile failed earlier.
- Fix earlier compiler errors first (especially `PIT_HZ`), then rebuild.
- If it still fails, verify naming consistency between `IDT.asm` externs and C handlers:
  - Assembly expects `_isr0_Handler`, `_isr1_Handler`, `_isr12_Handler`, `_isr14_Handler`
  - C defines `isr0_Handler`, `isr1_Handler`, `isr12_Handler`, `isr14_Handler`

**Symptom:** `objcopy: 'bin\\Kernel.img': No such file`
- `ld` did not produce `bin\\Kernel.img`, so `objcopy` has nothing to convert.
- This is a downstream error. Fix the first compiler/linker error above it and rerun the build.

**Symptom:** `boot_sectors.inc` contains only `%define SECTOR_COUNT` (no value)
- `bin\\kernel.bin` was not created (usually because link/objcopy failed), so size calculation was empty.
- Rebuild after fixing compile/link errors; the file should look like:
  - `%define SECTOR_COUNT <number>`

### Boot Problems

**Symptom:** QEMU shows "No bootable device"
- Check that `bin/os-image` exists and is 1.44 MB
- Verify boot signature `0xAA55` at bytes 510-511

**Symptom:** Boot stops at 'A' or 'B'
- Disk read is failing
- Check that `compile.bat` completed successfully

**Symptom:** Boot shows 'X'
- Disk read error occurred
- Try rebuilding from scratch

**Symptom:** Boot shows digits then 'X' (e.g., `AB1234567X`)
- A disk read crossed a 64 KB DMA boundary
- BIOS `int 0x13` cannot read sectors that span `0x10000`, `0x20000`, etc.
- Each read must stay within a single 64 KB region
- Split the offending read into two parts: one filling up to the boundary, one starting after it (with ES segment adjusted)

**Symptom:** Boot completes but screen is garbled
- Protected mode switch may have failed
- GDT might be corrupted

### Kernel Problems

**Symptom:** Kernel crashes immediately
- Check that `.rodata` is linked (strings will be missing otherwise)
- Verify stack pointer is set correctly

**Symptom:** Strings don't appear or new commands do nothing
- The kernel binary may have grown beyond what the bootloader loads
- Check `boot_sectors.inc` for the current `SECTOR_COUNT`
- Compare against the current bootloader maximum (197 sectors with existing `EXTRA1/2/3` reads)
- If the kernel grew beyond that, add more read operations in `Boot.asm` (watch for DMA boundaries!)

**Symptom:** Strings don't appear
- The linker script must include `.rodata` section
- Make sure `link.ld` is being used

**Symptom:** Keyboard doesn't work
- IDT may not be set up correctly
- PIC might not be remapped
- Check IRQ1 handler

### Filesystem Problems

**Symptom:** "File not found" for existing files
- Filesystem might be corrupted
- Delete `bin/hdd.img` and restart to reset

**Symptom:** Files disappear after reboot
- Make sure you saved (F2 in editor)
- Check that writes completed successfully

**Symptom:** EXEC fails with "not executable"
- Run `CHMODX filename` first
- Or use `INSTALL` for built-in programs

### Audio Problems

**Symptom:** No sound from BEEP
- QEMU needs audio enabled:
  ```
  qemu-system-i386 ... -audiodev dsound,id=snd0 -machine pcspk-audiodev=snd0
  ```

**Symptom:** Sound continues after BEEP
- Run `MUTE` to stop the speaker

### Network Problems

**Symptom:** `NETINFO` says NIC not initialized
- Ensure QEMU includes the RTL8029 device:
  - `-netdev user,id=net0 -device ne2k_pci,netdev=net0`
- Rebuild and reboot after updating `compile.bat`

**Symptom:** `NETTX` fails
- Run `NETINFO` first and confirm I/O base + MAC are valid
- If MAC is all zeros or `FF:FF:FF:FF:FF:FF`, NIC init failed

**Symptom:** `NETRX` always says no packet
- This command is poll-once; run it repeatedly while traffic exists
- Ensure there is live traffic on the emulated network; ARP/DHCP/ICMP/IPv4/UDP/TCP parsing is present, but shell receive/dequeue remains explicit command-driven

### Editor Problems

**Symptom:** Editor shows wrong characters
- Shift state might be stuck
- Restart ErisOS

**Symptom:** Can't type certain characters
- Some keys may not be mapped
- Check the scancode-to-ASCII table

### Assembler Problems

**Symptom:** "Unknown instruction" error
- The built-in assembler has limited support
- Check the [supported instructions list](ASSEMBLER_REF.md#supported-instructions)
- Use NASM for complex programs

**Symptom:** Program crashes when run
- Check calling convention (cdecl)
- Verify stack cleanup (`add esp, N` after calls)
- Make sure you return properly
