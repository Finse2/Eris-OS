# ErisOS Assembler And Editor Reference

Back to [README](README.md)

## Built-in Editor

Command:

```text
EDIT <filename>
```

Controls:

- Arrow keys: move cursor
- `F2`: save
- `ESC`: exit

Limits from current implementation:

- max lines: `50`
- max chars per line: `80`

## Built-in Assembler

Command:

```text
ASM <source.asm> <output.bin>
```

Get instruction help:

```text
HELP ASM
HELP ASM MOV
HELP ASM JNE
```

## Supported Instruction Set

### Data

- `MOV`
- `PUSH`
- `POP`
- `DB`

### Arithmetic

- `ADD`
- `SUB`
- `INC`
- `DEC`
- `NEG`

### Logic/Test

- `AND`
- `OR`
- `XOR`
- `NOT`
- `CMP`
- `TEST`

### Jumps

- unconditional: `JMP`
- eq/neq: `JE`/`JZ`, `JNE`/`JNZ`
- signed: `JG`, `JGE`, `JL`, `JLE`
- unsigned: `JA`, `JAE`, `JB`, `JBE`
- sign: `JS`, `JNS`

### Flow

- `CALL`
- `RET`
- `NOP`

## Not Supported

- `MUL`, `IMUL`, `DIV`, `IDIV`
- shift/rotate (`SHL`, `SHR`, `SAR`, `ROL`, `ROR`)
- `LOOP`
- macros/includes
- floating-point instructions

## Program ABI Reminder

Every executable program receives `KernelAPI*` as first arg.

```asm
push ebp
mov ebp, esp
mov eax, [ebp+8]   ; KernelAPI*
; ...
xor eax, eax
pop ebp
ret
```

## Typical Workflow

```text
EDIT HELLO.ASM
ASM HELLO.ASM HELLO.BIN
CHMODX HELLO.BIN
EXEC HELLO.BIN
```
