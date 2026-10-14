# ErisOS API And Filesystem

Back to [README](README.md)

## Filesystem Layout

On `bin\hdd.img`:

- Sector `0`: reserved/protected boot sector
- Sector `1`: FAT (`FS_FAT_ENTRIES = 128`)
- Sector `2`: directory table (`FS_MAX_FILES = 16`)
- Sector `3+`: data sectors

Constants from `filesystem.h`:

- Sector size: `512`
- Max files: `16`
- Max filename length: `12`
- FAT values:
  - free: `0x00000000`
  - end: `0xFFFFFFFF`
  - bad: `0xFFFFFFFE`

## File Commands

- `CREATE <filename>`
- `DELETE <filename>`
- `READ <filename>`
- `WRITE <filename> <data>`
- `LIST`
- `SIZE <filename>`
- `CHMODX <filename>`
- `SECTOR`

Notes:

- `WRITE` auto-creates the file if missing.
- Empty write is allowed (`WRITE X` with no data payload creates/clears file).
- `CREATE` enforces max filename length 12.

## System Files

ErisOS uses normal filesystem files for user-facing configuration.

### `ACCOUNT.TXT`

Created by first launch setup:

```text
USER=admin
PASS=eris
```

- Username comparison is case-insensitive.
- Password comparison is case-sensitive.
- If the file is missing, graphical boot starts first launch setup.

### `ERISCFG.TXT`

Created by first launch setup and updated by the Settings app:

```text
THEME=0
FPS=7
MOUSE=1
RES=0
TZ=0
```

Fields:

- `THEME`: `0` light/classic, `1` dark
- `FPS`: login/loading animation speed, clamped to `3..15`
- `MOUSE`: mouse sensitivity, clamped to `1..3`
- `RES`: display preference (`0` auto, `1` 1280x720, `2` 1024x768, `3` 800x600, `4` 640x480)
- `TZ`: timezone offset from UTC, clamped to `-12..14`

If this file is missing, graphical boot starts first launch setup.

## Program Execution Model

`EXEC <filename>` does:

1. locate file
2. verify executable flag (`CHMODX`)
3. validate size (`<= 64KB`)
4. load flat binary to `0x50000`
5. build `KernelAPI` struct
6. call program entry as `int (*)(KernelAPI*)`

Execution constants:

- `EXEC_LOAD_ADDR = 0x50000`
- `EXEC_MAX_SIZE = 0x10000` (64KB)

## KernelAPI (ABI)

```c
struct KernelAPI {
    void (*print_string)(char*);              // +0
    void (*print_char)(char);                 // +4
    void (*print_number)(unsigned int);       // +8
    void (*print_hex)(unsigned int);          // +12
    void (*cls)(void);                        // +16
    void (*beep)(unsigned int, unsigned int); // +20
    void (*set_color)(char);                  // +24
    int (*read_file)(char*, char*, unsigned int);  // +28
    int (*write_file)(char*, char*, unsigned int); // +32
    int (*get_file_size)(char*);              // +36
    void* (*kmalloc)(unsigned int);           // +40
    void (*kfree)(void*);                     // +44
};
```

Calling convention: `cdecl`.

- Program receives `KernelAPI*` as first argument (`[ebp+8]` in 32-bit ASM)
- Return value in `EAX`
- Caller cleans stack

## INSTALL Convenience Programs

`INSTALL <name>` writes prebuilt executables:

- `HELLO` -> `HELLO.BIN`
- `BEEP` -> `BEEP.BIN`
- `CLEAR` -> `CLEAR.BIN`
- any other token -> `TEST.BIN`

Each is automatically marked executable.

## Minimal Program (ASM)

```asm
push ebp
mov ebp, esp
mov eax, [ebp+8]      ; KernelAPI*
mov ebx, [eax]        ; print_string
push msg
call ebx
add esp, 4
xor eax, eax
pop ebp
ret
msg: db "Hello from EXEC", 10, 0
```
