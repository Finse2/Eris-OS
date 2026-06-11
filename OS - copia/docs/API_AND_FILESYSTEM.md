# ErisOS API & Filesystem Guide

> Working with files, running programs, the Kernel API, and writing your own programs.
>
> Back to [README](../README.md)


---

## Working with Files

ErisOS includes a simple filesystem for storing data persistently on disk.

### Filesystem Overview

The filesystem uses a FAT-like structure on the hard disk image (`bin/hdd.img`):

```
Sector 0:  Boot sector (protected, cannot be overwritten)
Sector 1:  FAT (File Allocation Table)
Sector 2:  Directory table
Sector 3+: Data sectors
```

### How It Works

**Directory Table:**
- Stores file metadata (name, size, starting sector, flags)
- Maximum 16 files
- Each filename can be up to 12 characters

**File Allocation Table (FAT):**
- Tracks which sectors belong to which file
- Each entry points to the next sector in a chain
- `0xFFFFFFFF` marks the end of a file

**Data Sectors:**
- Actual file content is stored here
- Each sector holds 512 bytes
- Files can span multiple sectors

### Filesystem Limits

| Limit | Value |
|-------|-------|
| Maximum files | 16 |
| Maximum filename length | 12 characters |
| Maximum file size | 64 KB |
| Total data space | 128 sectors x 512 bytes = 64 KB |

### File Operations in Practice

**Creating and writing a file:**
```
CREATE NOTES.TXT
WRITE NOTES.TXT Hello, this is my first file!
```

**Reading a file:**
```
READ NOTES.TXT
```

**Checking file size:**
```
SIZE NOTES.TXT
```

**Listing all files:**
```
LIST
```

**Deleting a file:**
```
DELETE NOTES.TXT
```

### Executable Files

To run a program, it must be marked as executable:

```
CHMODX MYPROGRAM.BIN
EXEC MYPROGRAM.BIN
```

The `INSTALL` command creates pre-built test programs that are already marked executable:

```
INSTALL HELLO    ; Creates HELLO.BIN - prints "Hi!"
INSTALL BEEP     ; Creates BEEP.BIN - plays a tone
INSTALL CLEAR    ; Creates CLEAR.BIN - clears screen
INSTALL TEST     ; Creates TEST.BIN - returns exit code 42
```

---

## Running Programs

ErisOS can load and execute binary programs from the filesystem.

### How Program Execution Works

When you run `EXEC MYPROGRAM.BIN`, the kernel:

1. **Finds the file** - Looks up the filename in the directory table
2. **Checks permissions** - Verifies the file is marked executable
3. **Loads the binary** - Copies the file contents to `0x50000`
4. **Sets up the API** - Prepares a structure with kernel function pointers
5. **Calls the program** - Jumps to address `0x50000` with the API pointer as an argument
6. **Returns control** - When the program returns, execution resumes in the shell

### Program Environment

| Property | Value |
|----------|-------|
| Load address | `0x50000` (320 KB mark) |
| Maximum size | 64 KB |
| Entry point | Start of file (offset 0) |
| Calling convention | cdecl (C calling convention) |
| First argument | Pointer to KernelAPI structure |
| Return value | Exit code in EAX |

---

## The Kernel API

Programs receive a pointer to this structure, giving them access to kernel functions:

```c
struct KernelAPI {
    void (*print_string)(char*);              // Offset +0
    void (*print_char)(char);                 // Offset +4
    void (*print_number)(unsigned int);       // Offset +8
    void (*print_hex)(unsigned int);          // Offset +12
    void (*cls)(void);                        // Offset +16
    void (*beep)(unsigned int, unsigned int); // Offset +20
    void (*set_color)(char);                  // Offset +24
    int (*read_file)(char*, char*, unsigned int);   // Offset +28
    int (*write_file)(char*, char*, unsigned int);  // Offset +32
    int (*get_file_size)(char*);              // Offset +36
    void* (*kmalloc)(unsigned int);           // Offset +40
    void (*kfree)(void*);                     // Offset +44
};
```

### API Function Reference

| Function | Offset | Arguments | Description |
|----------|--------|-----------|-------------|
| `print_string` | +0 | `char* str` | Print a null-terminated string |
| `print_char` | +4 | `char c` | Print a single character |
| `print_number` | +8 | `unsigned int n` | Print a decimal number |
| `print_hex` | +12 | `unsigned int n` | Print a hexadecimal number |
| `cls` | +16 | (none) | Clear the screen |
| `beep` | +20 | `freq, ms` | Play a tone |
| `set_color` | +24 | `char attr` | Set display color attribute |
| `read_file` | +28 | `name, buf, max` | Read file into buffer |
| `write_file` | +32 | `name, data, size` | Write buffer to file |
| `get_file_size` | +36 | `char* name` | Get file size in bytes |
| `kmalloc` | +40 | `unsigned int size` | Allocate heap memory (returns `void*` or `0` on failure) |
| `kfree` | +44 | `void* ptr` | Free previously allocated heap memory |

### Using the API from Assembly

In assembly, you access the API through pointer offsets:

```asm
mov eax, [ebp+8]      ; EAX = KernelAPI pointer
mov ebx, [eax]        ; EBX = print_string  (offset +0)
mov ecx, [eax+4]      ; ECX = print_char    (offset +4)
mov edx, [eax+8]      ; EDX = print_number  (offset +8)
; ...
mov ebx, [eax+40]     ; EBX = kmalloc       (offset +40)
mov ecx, [eax+44]     ; ECX = kfree         (offset +44)
```

---

## First Kernel Feature Guide

This mini tutorial shows how to go from "I can run programs" to "I can modify kernel behavior" by adding a tiny driver-like feature.

Goal: add a new shell command `TONE <freq>` that plays a short speaker tone at the requested frequency.

### Why this is a good first kernel change

- Touches command parsing (`extra.h`)
- Uses real hardware I/O (PIT + speaker ports)
- Keeps scope small and debuggable

### Step 1: Find the existing speaker control path

In `extra.h`, locate:
- `speakerOn(unsigned int freq)`
- `speakerOff()`
- `beep(unsigned int freq, unsigned int ms)`

These already drive:
- PIT channel 2 (`0x42`, `0x43`)
- Speaker gate (`0x61`)

### Step 2: Add a command token in the parser

In `strEval()` (also in `extra.h`), add a command constant near other command names:

```c
char cmdTone[] = "TONE";
```

### Step 3: Add the command handler logic

In the same function, add dispatch logic:

```c
else if(strcmp(command, cmdTone)) {
    char tok[16];
    char* p = argument;
    unsigned int freq;

    if(!nextToken(&p, tok, sizeof(tok)) || !parseUnsignedDec(tok, &freq)) {
        printString("\nUsage: TONE <freq>\n");
        printString("Example: TONE 440\n");
    } else {
        beep(freq, 200);  // 200 ms tone
    }
}
```

### Step 4: Update HELP text

Still in `strEval()`, update:
- the main command list (`HELP`)
- `HELP TONE` details

Keeping help text updated is part of "done."

### Step 5: Build and run

```batch
compile.bat
```

Then in ErisOS:

```text
TONE 440
TONE 880
```

### Step 6: Debug checklist if it fails

- Command not recognized: verify command token + dispatch branch were both added.
- No sound: confirm QEMU audio flags are enabled (see troubleshooting in `docs/INTERNAL_DOCS.md`).
- Build errors: check for typos in function names and missing semicolons in `extra.h`.

### Next step after this tutorial

Use the same workflow to add a tiny hardware status command (for example `PORTIN <hexport>`), then document it in:
- `docs/README.md` command table
- `docs/INTERNAL_DOCS.md` hardware section

---

## Writing Your Own Programs

You can write programs for ErisOS in either C or assembly language.

### Writing Programs in C

Here's a complete "Hello World" program in C:

```c
// hello.c - A simple ErisOS program

// Define the API structure (must match kernel's version)
struct KernelAPI {
    void (*print_string)(char*);
    void (*print_char)(char);
    void (*print_number)(unsigned int);
    void (*print_hex)(unsigned int);
    void (*cls)(void);
    void (*beep)(unsigned int, unsigned int);
    void (*set_color)(char);
    int (*read_file)(char*, char*, unsigned int);
    int (*write_file)(char*, char*, unsigned int);
    int (*get_file_size)(char*);
    void* (*kmalloc)(unsigned int);
    void (*kfree)(void*);
};

// Entry point - receives API pointer as first argument
int main(struct KernelAPI* api) {
    api->print_string("Hello from C!\n");
    return 0;  // Exit code
}
```

**Compiling C programs:**

You need a cross-compiler targeting i686-elf:

```bash
# Compile to object file
i686-elf-gcc -ffreestanding -nostdlib -c hello.c -o hello.o

# Link to flat binary at load address 0x50000
i686-elf-ld -Ttext=0x50000 --oformat binary -o hello.bin hello.o
```

**Compiler flags explained:**
- `-ffreestanding` - Don't assume a hosted environment
- `-nostdlib` - Don't link standard library
- `-Ttext=0x50000` - Set code start address
- `--oformat binary` - Output raw binary (not ELF)

### Writing Programs in Assembly (External)

For programs assembled outside ErisOS using NASM:

```asm
; hello.asm - ErisOS program (for NASM)
BITS 32
ORG 0x50000

; Entry point
_start:
    push ebp
    mov ebp, esp

    ; Get KernelAPI pointer from first argument
    mov eax, [ebp+8]

    ; Get print_string function (offset 0)
    mov ebx, [eax]

    ; Call print_string(message)
    push message
    call ebx
    add esp, 4

    ; Return 0
    mov eax, 0
    pop ebp
    ret

; Data section
message: db 'Hello from ASM!', 10, 0
```

**Assembling with NASM:**

```bash
nasm -f bin hello.asm -o hello.bin
```

### Writing Programs in Assembly (Built-in Assembler)

ErisOS has a built-in assembler, so you can write programs directly on the system!

**Example program using the built-in assembler:**

```asm
; hello.asm - For built-in ASM command

push ebp
mov ebp, esp
mov eax, [ebp+8]
mov ebx, [eax]
push msg
call ebx
add esp, 4
xor eax, eax
pop ebp
ret
msg: db "Hello from ErisOS!", 10, 0
```

**Complete workflow within ErisOS:**

```
EDIT HELLO.ASM           ; Create/edit the source file
; (write your code in the editor)
; Press F2 to save, ESC to exit

ASM HELLO.ASM HELLO.BIN  ; Assemble to binary
EXEC HELLO.BIN           ; Run the program
```

### Example: Looping Program (Built-in Assembler)

This program loops twice and prints "Hi!" each time:

```asm
; hiloop.asm - Prints "Hi!" twice using a loop

push ebp
mov ebp, esp

mov eax, [ebp+8]     ; Get KernelAPI pointer
mov ebx, [eax]       ; Get print_string (offset 0)

mov ecx, 2           ; Loop counter = 2

loop:
    push ecx          ; Save counter (CALL may change ECX)
    push msg          ; Push string address
    call ebx          ; Call print_string
    add esp, 4        ; Clean up argument
    pop ecx           ; Restore counter
    dec ecx           ; Counter: 2->1->0
    jnz loop          ; If not zero, loop again

xor eax, eax         ; Return 0
pop ebp
ret

msg: db "Hi!", 10, 0
```

**How it works:**
1. `mov ecx, 2` -- ECX is the loop counter, starts at 2
2. `push ecx` -- Save counter before the function call (ECX is a scratch register)
3. `push msg` / `call ebx` / `add esp, 4` -- Print "Hi!\n"
4. `pop ecx` -- Get counter back
5. `dec ecx` -- Decrement: 2->1, then 1->0
6. `jnz loop` -- If counter is NOT zero, jump back to `loop:`
7. After 2 iterations ECX hits 0, JNZ falls through, and we return

**Output:**
```
Hi!
Hi!
```

### Understanding the Calling Convention

Programs use the cdecl calling convention:

1. **Arguments pushed right-to-left** - Last argument pushed first
2. **Caller cleans up stack** - The calling code removes arguments after the call
3. **Return value in EAX** - Functions return integers in the EAX register
4. **EBX, ESI, EDI preserved** - Callees must save/restore these if used
5. **EAX, ECX, EDX are scratch** - Callees may freely modify these

**Stack layout when your program starts:**

```
[ebp+12] = (reserved)
[ebp+8]  = KernelAPI* pointer  <-- Your first argument
[ebp+4]  = Return address
[ebp+0]  = Saved EBP
```
