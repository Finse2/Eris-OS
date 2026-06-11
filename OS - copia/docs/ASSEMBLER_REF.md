# ErisOS Assembler Reference

> x86 instruction tables, ASCII reference, the built-in editor, and the built-in assembler.
>
> Back to [README](../README.md)

---

## x86 Assembly Quick Reference

### Register Overview

**32-bit General Purpose Registers:**

```
  EAX (Accumulator)      - Math operations, return values
  EBX (Base)             - General purpose, often preserved
  ECX (Counter)          - Loop counters, shift counts
  EDX (Data)             - I/O operations, multiplication
  ESP (Stack Pointer)    - Points to top of stack
  EBP (Base Pointer)     - Points to current stack frame
  ESI (Source Index)     - Source for string operations
  EDI (Destination Index)- Destination for string operations
```

### Instruction Reference Table

#### Data Movement Instructions

| Instruction | Syntax | Example | Description |
|-------------|--------|---------|-------------|
| `MOV` | `MOV dest, src` | `MOV EAX, 10` | Copy value from source to destination |
| `MOV` | `MOV reg, [mem]` | `MOV EAX, [EBX]` | Load value from memory address |
| `MOV` | `MOV [mem], reg` | `MOV [EBX], EAX` | Store value to memory address |
| `MOV` | `MOV reg, [reg+off]` | `MOV EAX, [EBP+8]` | Load from memory with offset |
| `PUSH` | `PUSH src` | `PUSH EAX` | Push value onto stack (ESP -= 4) |
| `POP` | `POP dest` | `POP EBX` | Pop value from stack (ESP += 4) |
| `XCHG` | `XCHG reg, reg` | `XCHG EAX, EBX` | Swap values between registers |
| `LEA` | `LEA reg, [mem]` | `LEA EAX, [EBX+4]` | Load effective address (calculate address) |

#### Arithmetic Instructions

| Instruction | Syntax | Example | Description |
|-------------|--------|---------|-------------|
| `ADD` | `ADD dest, src` | `ADD EAX, 5` | Add source to destination |
| `SUB` | `SUB dest, src` | `SUB ESP, 8` | Subtract source from destination |
| `INC` | `INC reg` | `INC ECX` | Increment by 1 |
| `DEC` | `DEC reg` | `DEC ECX` | Decrement by 1 |
| `MUL` | `MUL reg` | `MUL EBX` | Unsigned multiply (EAX x reg -> EDX:EAX) |
| `IMUL` | `IMUL reg` | `IMUL EBX` | Signed multiply |
| `DIV` | `DIV reg` | `DIV EBX` | Unsigned divide (EDX:EAX / reg) |
| `NEG` | `NEG reg` | `NEG EAX` | Two's complement negation |

#### Bitwise Logic Instructions

| Instruction | Syntax | Example | Description |
|-------------|--------|---------|-------------|
| `AND` | `AND dest, src` | `AND EAX, 0xFF` | Bitwise AND |
| `OR` | `OR dest, src` | `OR EAX, 0x01` | Bitwise OR |
| `XOR` | `XOR dest, src` | `XOR EAX, EAX` | Bitwise XOR (commonly used to zero a register) |
| `NOT` | `NOT reg` | `NOT EAX` | Bitwise NOT (invert all bits) |
| `SHL` | `SHL reg, count` | `SHL EAX, 2` | Shift left (multiply by 2^count) |
| `SHR` | `SHR reg, count` | `SHR EAX, 1` | Shift right logical (divide by 2^count) |
| `SAR` | `SAR reg, count` | `SAR EAX, 1` | Shift right arithmetic (preserves sign) |

#### Comparison & Flags

| Instruction | Syntax | Example | Description |
|-------------|--------|---------|-------------|
| `CMP` | `CMP op1, op2` | `CMP EAX, 10` | Compare (sets flags for conditional jumps) |
| `TEST` | `TEST op1, op2` | `TEST EAX, EAX` | Bitwise AND, result discarded (sets flags) |

#### Control Flow Instructions

| Instruction | Syntax | Example | Description |
|-------------|--------|---------|-------------|
| `JMP` | `JMP label` | `JMP start` | Unconditional jump |
| `JE` / `JZ` | `JE label` | `JE equal` | Jump if equal / zero |
| `JNE` / `JNZ` | `JNE label` | `JNE loop` | Jump if not equal / not zero |
| `JG` / `JNLE` | `JG label` | `JG greater` | Jump if greater (signed) |
| `JL` / `JNGE` | `JL label` | `JL less` | Jump if less (signed) |
| `JA` / `JNBE` | `JA label` | `JA above` | Jump if above (unsigned) |
| `JB` / `JNAE` | `JB label` | `JB below` | Jump if below (unsigned) |
| `CALL` | `CALL target` | `CALL function` | Call function (pushes return address) |
| `RET` | `RET` | `RET` | Return from function |
| `LOOP` | `LOOP label` | `LOOP start` | Decrement ECX, jump if not zero |
| `NOP` | `NOP` | `NOP` | No operation (does nothing) |

#### Stack Frame Instructions

| Instruction | Syntax | Example | Description |
|-------------|--------|---------|-------------|
| `ENTER` | `ENTER size, 0` | `ENTER 8, 0` | Create stack frame (PUSH EBP; MOV EBP,ESP; SUB ESP,size) |
| `LEAVE` | `LEAVE` | `LEAVE` | Destroy stack frame (MOV ESP,EBP; POP EBP) |

#### I/O Instructions

| Instruction | Syntax | Example | Description |
|-------------|--------|---------|-------------|
| `IN` | `IN AL, port` | `IN AL, 0x60` | Read byte from I/O port |
| `OUT` | `OUT port, AL` | `OUT 0x61, AL` | Write byte to I/O port |

#### Data Definition Directives

| Directive | Syntax | Example | Description |
|-----------|--------|---------|-------------|
| `DB` | `DB value(s)` | `DB "Hello", 0` | Define byte(s) |
| `DW` | `DW value(s)` | `DW 1234` | Define word(s) (2 bytes) |
| `DD` | `DD value(s)` | `DD 0x12345678` | Define double word(s) (4 bytes) |
| `TIMES` | `TIMES n DB val` | `TIMES 10 DB 0` | Repeat definition n times |

### Common Assembly Patterns

#### Function Prologue/Epilogue

```asm
; Standard function start (prologue)
my_function:
    push ebp          ; Save old base pointer
    mov ebp, esp      ; Set up new stack frame
    sub esp, 16       ; Reserve space for local variables

    ; ... function body ...

    ; Standard function end (epilogue)
    mov esp, ebp      ; Restore stack pointer
    pop ebp           ; Restore old base pointer
    ret               ; Return to caller
```

#### Accessing Function Arguments

```asm
; After prologue, stack looks like:
;   [EBP+12] = Third argument
;   [EBP+8]  = Second argument / First argument if only one
;   [EBP+4]  = Return address
;   [EBP+0]  = Saved EBP (old base pointer)

mov eax, [ebp+8]      ; Get first argument
mov ebx, [ebp+12]     ; Get second argument
```

#### Calling a Function

```asm
; Call printf("Result: %d", 42)
push 42               ; Second argument (pushed first)
push message          ; First argument (pushed last)
call printf           ; Call function
add esp, 8            ; Clean up stack (2 args x 4 bytes)
```

#### Loop Example

```asm
    mov ecx, 10       ; Loop counter = 10
loop_start:
    ; ... do something ...
    dec ecx           ; Decrement counter
    jnz loop_start    ; Jump if not zero
```

#### Zeroing a Register

```asm
xor eax, eax          ; EAX = 0 (faster than MOV EAX, 0)
```

---

## ASCII Reference

### Control Characters

| Char | Name | Dec | Hex | Use in Assembly |
|------|------|-----|-----|-----------------|
| NUL | Null | 0 | 0x00 | String terminator: `db "Hello", 0` |
| TAB | Tab | 9 | 0x09 | Horizontal tab: `db 9` |
| LF | Line Feed | 10 | 0x0A | Newline (Unix): `db "Hi!", 10, 0` |
| CR | Carriage Return | 13 | 0x0D | Return to line start: `db 13, 10` |
| ESC | Escape | 27 | 0x1B | Escape sequences |

### Numbers (0-9)

| Char | Dec | Hex | Char | Dec | Hex | Char | Dec | Hex |
|------|-----|-----|------|-----|-----|------|-----|-----|
| `0` | 48 | 0x30 | `4` | 52 | 0x34 | `8` | 56 | 0x38 |
| `1` | 49 | 0x31 | `5` | 53 | 0x35 | `9` | 57 | 0x39 |
| `2` | 50 | 0x32 | `6` | 54 | 0x36 | | | |
| `3` | 51 | 0x33 | `7` | 55 | 0x37 | | | |

**Tip:** To convert digit char to number: `SUB AL, 48` or `SUB AL, '0'`

### Uppercase Letters (A-Z)

| Char | Dec | Hex | Char | Dec | Hex | Char | Dec | Hex |
|------|-----|-----|------|-----|-----|------|-----|-----|
| `A` | 65 | 0x41 | `J` | 74 | 0x4A | `S` | 83 | 0x53 |
| `B` | 66 | 0x42 | `K` | 75 | 0x4B | `T` | 84 | 0x54 |
| `C` | 67 | 0x43 | `L` | 76 | 0x4C | `U` | 85 | 0x55 |
| `D` | 68 | 0x44 | `M` | 77 | 0x4D | `V` | 86 | 0x56 |
| `E` | 69 | 0x45 | `N` | 78 | 0x4E | `W` | 87 | 0x57 |
| `F` | 70 | 0x46 | `O` | 79 | 0x4F | `X` | 88 | 0x58 |
| `G` | 71 | 0x47 | `P` | 80 | 0x50 | `Y` | 89 | 0x59 |
| `H` | 72 | 0x48 | `Q` | 81 | 0x51 | `Z` | 90 | 0x5A |
| `I` | 73 | 0x49 | `R` | 82 | 0x52 | | | |

### Lowercase Letters (a-z)

| Char | Dec | Hex | Char | Dec | Hex | Char | Dec | Hex |
|------|-----|-----|------|-----|-----|------|-----|-----|
| `a` | 97  | 0x61 | `j` | 106 | 0x6A | `s` | 115 | 0x73 |
| `b` | 98  | 0x62 | `k` | 107 | 0x6B | `t` | 116 | 0x74 |
| `c` | 99  | 0x63 | `l` | 108 | 0x6C | `u` | 117 | 0x75 |
| `d` | 100 | 0x64 | `m` | 109 | 0x6D | `v` | 118 | 0x76 |
| `e` | 101 | 0x65 | `n` | 110 | 0x6E | `w` | 119 | 0x77 |
| `f` | 102 | 0x66 | `o` | 111 | 0x6F | `x` | 120 | 0x78 |
| `g` | 103 | 0x67 | `p` | 112 | 0x70 | `y` | 121 | 0x79 |
| `h` | 104 | 0x68 | `q` | 113 | 0x71 | `z` | 122 | 0x7A |
| `i` | 105 | 0x69 | `r` | 114 | 0x72 | | | |

**Tip:** To convert uppercase to lowercase: `OR AL, 0x20` (add 32)
**Tip:** To convert lowercase to uppercase: `AND AL, 0xDF` (subtract 32)

### Symbols & Punctuation

| Char | Dec | Hex  | Char | Dec | Hex  | Char| Dec| Hex |
|------|-----|----- |------|-----|------|-----|----|-----|
| ` `  | 32  | 0x20 | `+`  | 43  | 0x2B | `:` | 58 | 0x3A|
| `!`  | 33  | 0x21 | `,`  | 44  | 0x2C | `;` | 59 | 0x3B|
| `"`  | 34  | 0x22 | `-`  | 45  | 0x2D | `<` | 60 | 0x3C|
| `#`  | 35  | 0x23 | `.`  | 46  | 0x2E | `=` | 61 | 0x3D|
| `$`  | 36  | 0x24 | `/`  | 47  | 0x2F | `>` | 62 | 0x3E|
| `%`  | 37  | 0x25 | `?`  | 63  | 0x3F |
| `&`  | 38  | 0x26 |`@`   | 64  | 0x40 |
| `'`  | 39  | 0x27 |
| `(`  | 40  | 0x28 | `[`  | 91  | 0x5B | `{` | 123| 0x7B|
| `)`  | 41  | 0x29 | `\`  | 92  | 0x5C | `\|`| 124| 0x7C|
| `*`  | 42  | 0x2A | `]`  | 93  | 0x5D | `}` | 125| 0x7D|
| `^`  | 94  | 0x5E | `~` | 126 | 0x7E |
| `_`  | 95  | 0x5F |
| | | | `` ` `` | 96 | 0x60 |

### Common Assembly Examples

```asm
; Defining strings with special characters
msg:    db "Hello, World!", 10, 0     ; String with newline + null terminator
prompt: db "Enter name: ", 0          ; String with space
path:   db "C:\file.txt", 0           ; Backslash in path
quote:  db 'He said "Hi"', 0          ; Double quotes inside single quotes

; Character comparisons
cmp al, 32          ; Is it a space?
cmp al, 10          ; Is it newline?
cmp al, 0           ; Is it null terminator?

; Character range checks
cmp al, 48          ; al >= '0' ?
cmp al, 57          ; al <= '9' ? (is digit)
cmp al, 65          ; al >= 'A' ?
cmp al, 90          ; al <= 'Z' ? (is uppercase)
```

---

## Flags Register Reference

| Flag | Name | Set When |
|------|------|----------|
| ZF | Zero Flag | Result is zero |
| SF | Sign Flag | Result is negative |
| CF | Carry Flag | Unsigned overflow occurred |
| OF | Overflow Flag | Signed overflow occurred |

---

## The Built-in Editor

ErisOS includes a full-screen text editor called with the `EDIT` command.

### Starting the Editor

```
EDIT MYFILE.TXT
```

If the file exists, it opens for editing. If not, a new file is created.

### Editor Interface

The editor uses a nano-style interface:

```
+------------------------------------------------------------------------------+
| EDIT: HELLO.ASM                                              Line: 1  F2=Save|
+------------------------------------------------------------------------------+
|                                                                              |
| ; Your code here                                                             |
| push ebp                                                                     |
| mov ebp, esp                                                                 |
| ...                                                                          |

```

- **Blue background** - Editing area
- **Status bar** - Shows filename, line number, and shortcuts
- **80x24 editing area** - Standard terminal size

### Editor Controls

**Navigation:**
| Key | Action |
|-----|--------|
| Up Arrow | Move cursor up |
| Down Arrow | Move cursor down |
| Left Arrow | Move cursor left |
| Right Arrow | Move cursor right |

**Editing:**
| Key | Action |
|-----|--------|
| Letters/Numbers | Insert character at cursor |
| Shift + Letter | Insert uppercase letter |
| Shift + Number | Insert symbol (!@#$%^&*()) |
| Backspace | Delete character before cursor |
| Enter | Insert new line |
| Tab | Insert 4 spaces |

**File Operations:**
| Key | Action |
|-----|--------|
| F2 | Save file to disk |
| ESC | Exit editor (prompts if unsaved) |

### Shift Key Mappings

The editor supports the full US keyboard layout:

**Number row with Shift:**
| Key | Normal | Shifted |
|-----|--------|---------|
| 1 | `1` | `!` |
| 2 | `2` | `@` |
| 3 | `3` | `#` |
| 4 | `4` | `$` |
| 5 | `5` | `%` |
| 6 | `6` | `^` |
| 7 | `7` | `&` |
| 8 | `8` | `*` |
| 9 | `9` | `(` |
| 0 | `0` | `)` |

**Symbol keys with Shift:**
| Key | Normal | Shifted |
|-----|--------|---------|
| `-` | `-` | `_` |
| `=` | `=` | `+` |
| `[` | `[` | `{` |
| `]` | `]` | `}` |
| `;` | `;` | `:` |
| `'` | `'` | `"` |
| `,` | `,` | `<` |
| `.` | `.` | `>` |
| `/` | `/` | `?` |
| `\` | `\` | `\|` |
| `` ` `` | `` ` `` | `~` |

### Editor Limitations

- Maximum 50 lines per file
- Maximum ~80 characters per line
- No copy/paste functionality
- No search functionality
- No undo (be careful!)

---

## The Built-in Assembler

ErisOS includes a simple x86 assembler that can compile assembly code to executable binaries.

### Using the Assembler

```
ASM <source.asm> <output.bin>
```

**Example:**
```
ASM HELLO.ASM HELLO.BIN
```

On success, you'll see:
```
[SUCCESS] Assembled 42 bytes to HELLO.BIN
```

### Getting Help on Instructions

Use `HELP ASM <instruction>` to get detailed help on any instruction:

```
HELP ASM MOV       ; Learn about MOV instruction
HELP ASM JNE       ; Learn about conditional jumps
HELP ASM CMP       ; Learn about comparisons
HELP ASM DB        ; Learn about defining data
```

Each help entry shows:
- What the instruction does
- Syntax variations
- Practical examples
- Which flags are affected

### Supported Instructions

The assembler supports a rich subset of x86 instructions:

**Data Movement:**
| Instruction | Example | Description |
|-------------|---------|-------------|
| `MOV reg, imm` | `MOV EAX, 42` | Load immediate value |
| `MOV reg, reg` | `MOV EAX, EBX` | Copy register |
| `MOV reg, [reg]` | `MOV EAX, [EBX]` | Load from memory |
| `MOV reg, [reg+off]` | `MOV EAX, [EBP+8]` | Load with offset |

**Stack Operations:**
| Instruction | Example | Description |
|-------------|---------|-------------|
| `PUSH reg` | `PUSH EAX` | Push register |
| `PUSH imm` | `PUSH 42` | Push immediate |
| `PUSH label` | `PUSH msg` | Push label address |
| `POP reg` | `POP EBX` | Pop to register |

**Arithmetic:**
| Instruction | Example | Description |
|-------------|---------|-------------|
| `ADD reg, imm` | `ADD ESP, 4` | Add immediate |
| `SUB reg, imm` | `SUB ESP, 8` | Subtract immediate |
| `INC reg` | `INC ECX` | Increment by 1 |
| `DEC reg` | `DEC ECX` | Decrement by 1 |
| `NEG reg` | `NEG EAX` | Two's complement negate |

**Bitwise Logic:**
| Instruction | Example | Description |
|-------------|---------|-------------|
| `AND reg, reg/imm` | `AND EAX, 0xFF` | Bitwise AND |
| `OR reg, reg/imm` | `OR EAX, 0x01` | Bitwise OR |
| `XOR reg, reg` | `XOR EAX, EAX` | Bitwise XOR (zero a register) |
| `NOT reg` | `NOT EAX` | Bitwise NOT (invert bits) |

**Comparison:**
| Instruction | Example | Description |
|-------------|---------|-------------|
| `CMP reg, reg/imm` | `CMP EAX, 10` | Compare (sets flags) |
| `TEST reg, reg/imm` | `TEST EAX, EAX` | Bitwise AND test (sets flags) |

**Conditional Jumps:**
| Instruction | Aliases | Description |
|-------------|---------|-------------|
| `JMP label` | | Unconditional jump |
| `JE label` | `JZ` | Jump if equal / zero |
| `JNE label` | `JNZ` | Jump if not equal / not zero |
| `JG label` | `JNLE` | Jump if greater (signed) |
| `JGE label` | `JNL` | Jump if greater or equal (signed) |
| `JL label` | `JNGE` | Jump if less (signed) |
| `JLE label` | `JNG` | Jump if less or equal (signed) |
| `JA label` | `JNBE` | Jump if above (unsigned) |
| `JAE label` | `JNB, JNC` | Jump if above or equal (unsigned) |
| `JB label` | `JNAE, JC` | Jump if below (unsigned) |
| `JBE label` | `JNA` | Jump if below or equal (unsigned) |
| `JS label` | | Jump if sign (negative) |
| `JNS label` | | Jump if not sign (positive) |

**Control Flow:**
| Instruction | Example | Description |
|-------------|---------|-------------|
| `CALL reg` | `CALL EBX` | Call function via register |
| `RET` | `RET` | Return from function |
| `NOP` | `NOP` | No operation |

**Data Definition:**
| Directive | Example | Description |
|-----------|---------|-------------|
| `DB` | `DB "Hi!", 10, 0` | Define bytes (string + values) |

### Supported Registers

| Register | Purpose (by convention) |
|----------|------------------------|
| `EAX` | Accumulator, return values |
| `EBX` | Base register, preserved |
| `ECX` | Counter, scratch |
| `EDX` | Data, scratch |
| `ESP` | Stack pointer |
| `EBP` | Base pointer (frame pointer) |
| `ESI` | Source index, preserved |
| `EDI` | Destination index, preserved |

### Labels

Labels mark positions in your code. They must end with a colon:

```asm
start:
    push ebp
    ; ... code ...

msg:
    db "Hello", 0
```

Labels can be used with `PUSH` to get their address:

```asm
push msg        ; Push address of msg label
call ebx        ; Call print function
```

### Example: Hello World

```asm
; hello.asm - Prints "Hi!" using print_string

push ebp
mov ebp, esp
mov eax, [ebp+8]     ; Get KernelAPI pointer
mov ebx, [eax]       ; Get print_string (offset 0)
push msg
call ebx
add esp, 4
xor eax, eax
pop ebp
ret

msg: db "Hi!", 10, 0
```

### Example: Loop with Conditional Jump

```asm
; count.asm - Print numbers 1 to 5
push ebp
mov ebp, esp

mov eax, [ebp+8]      ; Get KernelAPI pointer
mov esi, eax          ; Save API pointer in ESI
mov ebx, [esi+4]      ; Get print_char function

mov ecx, 49           ; Start with '1' (ASCII 49)

loop_start:
    push ecx          ; Save counter
    push ecx          ; Push char to print
    call ebx          ; Call print_char
    add esp, 4        ; Clean stack
    pop ecx           ; Restore counter

    inc ecx           ; Next character
    cmp ecx, 54       ; Compare with '6' (ASCII 54)
    jl loop_start     ; Jump if less

    ; Print newline
    push 10
    call ebx
    add esp, 4

xor eax, eax
pop ebp
ret
```

### Assembler Limitations

The built-in assembler is intentionally simple. It does **not** support:

- Multiplication/division (MUL, DIV, IMUL, IDIV)
- Shift operations (SHL, SHR, SAR, ROL, ROR)
- Memory-to-memory operations
- Segment registers
- Floating point operations
- Macros or includes
- LOOP instruction

For more complex programs, use an external assembler like NASM (for now).
