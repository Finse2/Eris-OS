# ErisOS Assembler Code Examples

> Practical programs you can write, assemble, and run entirely inside ErisOS.
>
> Back to [README](../README.md)

---

## Quick Start Workflow

Every example in this document follows the same three-step process inside ErisOS:

```
EDIT MYFILE.ASM          ; Step 1 - Write the code (F2 to save, ESC to exit)
ASM MYFILE.ASM MYFILE.BIN  ; Step 2 - Assemble it
EXEC MYFILE.BIN          ; Step 3 - Run it
```

If the assembler reports an error, go back to `EDIT`, fix the line, save, and re-assemble.

---

## Example 1: Hello World

The simplest possible program. Prints a message and exits.

```asm
; hello.asm - Your first ErisOS program
;
; What it does:
;   Prints "Hello, ErisOS!" and returns.

push ebp
mov ebp, esp

mov eax, [ebp+8]       ; EAX = KernelAPI pointer
mov ebx, [eax]          ; EBX = print_string (offset +0)

push msg                ; Push address of our string
call ebx                ; Call print_string
add esp, 4              ; Clean up the argument

xor eax, eax            ; Return 0 (success)
pop ebp
ret

msg: db "Hello, ErisOS!", 10, 0
```

**How it works:**
1. `[ebp+8]` is the KernelAPI pointer the kernel passes to every program
2. `[eax]` at offset +0 is the `print_string` function
3. We push the address of our string, call the function, then clean up the stack
4. `xor eax, eax` is a fast way to set EAX to 0 (our exit code)

---

## Example 2: Print Multiple Lines

Shows how to call `print_string` multiple times with different messages.

```asm
; lines.asm - Print several lines of text
;
; What it does:
;   Prints three separate lines to the screen.

push ebp
mov ebp, esp

mov eax, [ebp+8]       ; EAX = KernelAPI pointer
mov edi, [eax]          ; EDI = print_string (preserved register)

; Print line 1
push line1
call edi
add esp, 4

; Print line 2
push line2
call edi
add esp, 4

; Print line 3
push line3
call edi
add esp, 4

xor eax, eax
pop ebp
ret

line1: db "=== ErisOS Program ===", 10, 0
line2: db "This prints three lines.", 10, 0
line3: db "All done!", 10, 0
```

**Key lesson:** EDI is a *preserved register* -- the called function won't overwrite it. That's why we store `print_string` in EDI instead of EBX here: both work because both are callee-saved.

---

## Example 3: Counting to 9

Prints the digits 1 through 9 using a loop and `print_char`.

```asm
; count.asm - Print digits 1 through 9
;
; What it does:
;   Loops from ASCII '1' (49) to '9' (57),
;   printing each character followed by a space.

push ebp
mov ebp, esp

mov eax, [ebp+8]       ; EAX = KernelAPI pointer
mov edi, [eax+4]        ; EDI = print_char (offset +4)

mov ecx, 49             ; ECX = ASCII '1'

print_digit:
    push ecx            ; Save counter
    push ecx            ; Push char argument
    call edi            ; Call print_char
    add esp, 4          ; Clean up argument

    ; Print a space between digits
    push 32             ; ASCII space
    call edi
    add esp, 4

    pop ecx             ; Restore counter
    inc ecx             ; Next digit character
    cmp ecx, 58         ; Past '9' (ASCII 57)?
    jl print_digit      ; If not, keep going

; Print a newline at the end
push 10
call edi
add esp, 4

xor eax, eax
pop ebp
ret
```

**Output:**
```
1 2 3 4 5 6 7 8 9
```

**Key lesson:** ECX is a scratch register (functions can clobber it), so we `push ecx` before calling and `pop ecx` after. This save/restore pattern is essential in loops.

---

## Example 4: Alphabet Printer

Prints the entire uppercase alphabet.

```asm
; alpha.asm - Print A-Z
;
; What it does:
;   Loops from ASCII 'A' (65) to 'Z' (90)
;   and prints each letter.

push ebp
mov ebp, esp

mov eax, [ebp+8]       ; KernelAPI pointer
mov edi, [eax+4]        ; print_char

mov ecx, 65             ; ECX = 'A'

next_letter:
    push ecx
    push ecx            ; Push letter to print
    call edi
    add esp, 4
    pop ecx

    inc ecx
    cmp ecx, 91         ; Past 'Z'?
    jl next_letter

; Newline
push 10
call edi
add esp, 4

xor eax, eax
pop ebp
ret
```

**Output:**
```
ABCDEFGHIJKLMNOPQRSTUVWXYZ
```

---

## Example 5: Countdown Timer

Counts down from 5 to 1, then prints "Go!".

```asm
; cdown.asm - Countdown from 5 to 1
;
; What it does:
;   Prints 5, 4, 3, 2, 1, Go! using a
;   decrementing loop with print_char.

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax+4]        ; print_char
mov esi, [eax]          ; print_string

mov ecx, 53             ; ASCII '5'

countdown:
    push ecx
    push ecx            ; Print the digit
    call edi
    add esp, 4

    push 46             ; Print '.' after each digit
    call edi
    add esp, 4

    push 46             ; Print another '.'
    call edi
    add esp, 4

    push 10             ; Newline
    call edi
    add esp, 4

    pop ecx
    dec ecx
    cmp ecx, 48         ; Below '0'?
    jg countdown

; Print "Go!"
push go_msg
call esi
add esp, 4

xor eax, eax
pop ebp
ret

go_msg: db "Go!", 10, 0
```

**Output:**
```
5..
4..
3..
2..
1..
Go!
```

---

## Example 6: ASCII Art Box

Draws a simple box made of characters.

```asm
; box.asm - Draw a box
;
; What it does:
;   Uses print_string to draw a text box.

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string

push top
call edi
add esp, 4

push mid1
call edi
add esp, 4

push mid2
call edi
add esp, 4

push mid3
call edi
add esp, 4

push bot
call edi
add esp, 4

xor eax, eax
pop ebp
ret

top:  db "+----------------+", 10, 0
mid1: db "|   ErisOS 1.0   |", 10, 0
mid2: db "|  Assembly is   |", 10, 0
mid3: db "|     fun!       |", 10, 0
bot:  db "+----------------+", 10, 0
```

**Output:**
```
+----------------+
|   ErisOS 1.0   |
|  Assembly is   |
|     fun!       |
+----------------+
```

---

## Example 7: Number Printer

Uses `print_number` to display decimal numbers.

```asm
; nums.asm - Print numbers using print_number
;
; What it does:
;   Demonstrates the print_number API call
;   to print integers in decimal.

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string
mov esi, [eax+8]        ; print_number (offset +8)
mov ebx, [eax+4]        ; print_char

; Print "Count: 42"
push label1
call edi
add esp, 4

push 42
call esi
add esp, 4

push 10                 ; newline
call ebx
add esp, 4

; Print "Hex offset: 256"
push label2
call edi
add esp, 4

push 256
call esi
add esp, 4

push 10
call ebx
add esp, 4

; Print "Max byte: 255"
push label3
call edi
add esp, 4

push 255
call esi
add esp, 4

push 10
call ebx
add esp, 4

xor eax, eax
pop ebp
ret

label1: db "Count: ", 0
label2: db "Hex offset: ", 0
label3: db "Max byte: ", 0
```

**Output:**
```
Count: 42
Hex offset: 256
Max byte: 255
```

---

## Example 8: Color Changer

Changes the text color before printing, then restores it.

```asm
; color.asm - Print text in different colors
;
; What it does:
;   Uses set_color (offset +24) to change the
;   display color, prints messages, then resets.
;
; Color byte format: 0xBF
;   B = background (0-7), F = foreground (0-15)
;   0x0F = white on black
;   0x0A = green on black
;   0x0C = red on black
;   0x0E = yellow on black
;   0x1F = white on blue

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string
mov esi, [eax+24]       ; set_color (offset +24)

; Set green text
push 0x0A
call esi
add esp, 4

push green_msg
call edi
add esp, 4

; Set red text
push 0x0C
call esi
add esp, 4

push red_msg
call edi
add esp, 4

; Set yellow text
push 0x0E
call esi
add esp, 4

push yellow_msg
call edi
add esp, 4

; Reset to white on black
push 0x0F
call esi
add esp, 4

push reset_msg
call edi
add esp, 4

xor eax, eax
pop ebp
ret

green_msg:  db "[OK] System nominal", 10, 0
red_msg:    db "[!!] Alert detected", 10, 0
yellow_msg: db "[..] Standing by", 10, 0
reset_msg:  db "Color reset.", 10, 0
```

**Key lesson:** The color attribute byte has the format `0xBF` where B = background color (0-7) and F = foreground color (0-15). See the color table in [INTERNAL_DOCS.md](INTERNAL_DOCS.md#vga-text-mode).

---

## Example 9: Beep Song

Plays a short sequence of tones using the PC speaker.

```asm
; song.asm - Play a short melody
;
; What it does:
;   Uses the beep function (offset +20) to play
;   several notes. beep(frequency, duration_ms).

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax+20]       ; beep (offset +20)
mov esi, [eax]          ; print_string

push playing
call esi
add esp, 4

; Note 1: C4 (262 Hz, 300 ms)
push 300
push 262
call edi
add esp, 8

; Note 2: E4 (330 Hz, 300 ms)
push 300
push 330
call edi
add esp, 8

; Note 3: G4 (392 Hz, 300 ms)
push 300
push 392
call edi
add esp, 8

; Note 4: C5 (523 Hz, 600 ms)
push 600
push 523
call edi
add esp, 8

push done
call esi
add esp, 4

xor eax, eax
pop ebp
ret

playing: db "Playing melody...", 10, 0
done:    db "Done!", 10, 0
```

**Note:** Sound requires QEMU to be started with audio enabled. The `beep` function takes two arguments: frequency in Hz (pushed first) and duration in milliseconds (pushed second) -- remember, cdecl pushes right-to-left.

---

## Example 10: Clear & Greet

Clears the screen, sets a color, and displays a centered greeting.

```asm
; greet.asm - Clear screen and greet the user
;
; What it does:
;   Clears the screen, sets blue-on-white color,
;   prints a greeting, then resets color.

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string
mov esi, [eax+16]       ; cls (offset +16)
mov ebx, [eax+24]       ; set_color (offset +24)

; Clear the screen
call esi                ; cls takes no arguments

; Set white on blue
push 0x1F
call ebx
add esp, 4

; Print greeting
push line1
call edi
add esp, 4

push line2
call edi
add esp, 4

push line3
call edi
add esp, 4

; Reset to default white on black
push 0x0F
call ebx
add esp, 4

xor eax, eax
pop ebp
ret

line1: db 10, 10, 0
line2: db "       Welcome to ErisOS!", 10, 0
line3: db "    Written entirely from scratch.", 10, 10, 0
```

---

## Example 11: Uppercase Converter

Takes a hardcoded lowercase string and prints it in uppercase, one character at a time.

```asm
; upper.asm - Convert and print lowercase to uppercase
;
; What it does:
;   Walks through a string byte by byte.
;   If a character is lowercase (97-122), it
;   subtracts 32 to make it uppercase.
;   Demonstrates memory access with [reg+offset].

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax+4]        ; print_char
mov esi, [eax]          ; print_string

; Print label
push label
call esi
add esp, 4

; ECX will be our index into the string
xor ecx, ecx            ; ECX = 0

convert_loop:
    ; Load the character at str[ECX]
    ; We use LEA to compute the address, then MOV to load
    ; But the built-in assembler supports MOV reg, [reg+off]
    ; So we'll use a different approach: keep a pointer in EDX

    ; Skip -- we'll use a simpler pointer approach
    jmp start

start:
    ; Load pointer to our string into EDX
    push msg
    pop edx              ; EDX = address of msg

next_char:
    mov eax, [edx]       ; Load 4 bytes (we only care about AL)
    and eax, 0xFF        ; Mask to get just the low byte

    ; Check for null terminator
    cmp eax, 0
    je done

    ; Check if lowercase: 'a' (97) to 'z' (122)
    cmp eax, 97
    jl not_lower
    cmp eax, 122
    jg not_lower

    ; Convert to uppercase by subtracting 32
    sub eax, 32

not_lower:
    ; Print the character
    push edx             ; Save our pointer
    push eax             ; Push character
    call edi             ; print_char
    add esp, 4
    pop edx              ; Restore pointer

    ; Move to next character
    inc edx
    jmp next_char

done:
    ; Print newline
    push 10
    call edi
    add esp, 4

    xor eax, eax
    pop ebp
    ret

label: db "Uppercase: ", 0
msg:   db "hello world from eris", 0
```

**Output:**
```
Uppercase: HELLO WORLD FROM ERIS
```

**Key lesson:** This demonstrates character-by-character string processing. We read from memory with `mov eax, [edx]`, mask to one byte with `and eax, 0xFF`, and use `cmp`/`jl`/`jg` to check if it's a lowercase letter.

---

## Example 12: Multiplication via Addition

Since the built-in assembler doesn't support `MUL`, we multiply using repeated addition.

```asm
; mult.asm - Multiply two numbers using addition
;
; What it does:
;   Computes 7 x 6 = 42 by adding 7 to itself
;   6 times, then prints the result.

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string
mov esi, [eax+8]        ; print_number
mov ebx, [eax+4]        ; print_char

; Print "7 x 6 = "
push label
call edi
add esp, 4

; Multiply: result = 7 * 6
; EAX = accumulator (result), ECX = counter
xor eax, eax            ; EAX = 0 (result)
mov ecx, 6              ; ECX = multiplier (loop 6 times)

multiply:
    add eax, 7          ; Add 7 each iteration
    dec ecx
    jnz multiply        ; Loop until ECX = 0

; EAX now holds 42
; Save it and print
push eax                ; Save result
push eax                ; Push for print_number
call esi
add esp, 4
pop eax                 ; Discard saved copy

; Newline
push 10
call ebx
add esp, 4

xor eax, eax
pop ebp
ret

label: db "7 x 6 = ", 0
```

**Output:**
```
7 x 6 = 42
```

**Key lesson:** When an instruction isn't available, think about what it does mathematically. Multiplication is just repeated addition. Division could be done with repeated subtraction.

---

## Example 13: Repeat String N Times

A generalized loop that prints the same message a configurable number of times.

```asm
; repeat.asm - Print a message 5 times
;
; What it does:
;   Uses a counter to print a numbered list.
;   Combines print_char and print_string in a loop.

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string
mov esi, [eax+4]        ; print_char

mov ecx, 1              ; Line number (start at 1)

print_loop:
    push ecx            ; Save counter

    ; Print the line number as a character
    ; Convert number 1-5 to ASCII '1'-'5' by adding 48
    mov eax, ecx
    add eax, 48
    push eax
    call esi            ; print_char(digit)
    add esp, 4

    ; Print ". " separator
    push sep
    call edi
    add esp, 4

    ; Print the message
    push msg
    call edi
    add esp, 4

    pop ecx             ; Restore counter
    inc ecx
    cmp ecx, 6          ; Done after 5?
    jl print_loop

xor eax, eax
pop ebp
ret

sep: db ". ", 0
msg: db "ErisOS is running!", 10, 0
```

**Output:**
```
1. ErisOS is running!
2. ErisOS is running!
3. ErisOS is running!
4. ErisOS is running!
5. ErisOS is running!
```

---

## Example 14: File Reader

Reads a file from disk and prints its contents using the Kernel API.

```asm
; fread.asm - Read and display a file
;
; What it does:
;   Uses read_file (offset +28) to load a file
;   into a buffer, then prints it.
;
; Before running, create a file to read:
;   CREATE TEST.TXT
;   WRITE TEST.TXT Hello from the filesystem!

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string (offset +0)
mov esi, [eax+28]       ; read_file (offset +28)

; Print header
push header
call edi
add esp, 4

; Call read_file(filename, buffer, max_size)
; Arguments pushed right-to-left (cdecl)
push 64                ; max_size = 64 bytes
push buffer            ; destination buffer
push fname             ; filename
call esi               ; read_file
add esp, 12            ; Clean up 3 arguments

; Check return value (EAX)
; EAX > 0 means bytes read, EAX <= 0 means error
cmp eax, 0
jle read_error

; Print the buffer contents
push buffer
call edi
add esp, 4

; Trailing newline
mov eax, [ebp+8]
mov ebx, [eax+4]       ; print_char
push 10
call ebx
add esp, 4

xor eax, eax
pop ebp
ret

read_error:
push errmsg
call edi
add esp, 4
mov eax, 1              ; Return error code 1
pop ebp
ret

header: db "--- File Contents ---", 10, 0
errmsg: db "Error: could not read file!", 10, 0
fname:  db "TEST.TXT", 0

; Reserve space for the file buffer
; (64 bytes in this example)
buffer: db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
        db 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
```

**Before running:** Make sure the file exists:
```
CREATE TEST.TXT
WRITE TEST.TXT Hello from the filesystem!
```

**Key lesson:** The `read_file` function takes three arguments (filename, buffer, max_size). The return value in EAX tells us how many bytes were read, or an error if <= 0.

---

## Example 15: File Writer

Writes data to a file using the Kernel API.

```asm
; fwrite.asm - Write data to a file
;
; What it does:
;   Uses write_file (offset +32) to save a
;   string to a file on disk.
;
; After running, verify with: READ OUTPUT.TXT

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string
mov esi, [eax+32]       ; write_file (offset +32)

; Write our data to the file
; write_file(filename, data, size)
push 28                ; Size: 28 bytes
push data              ; Data to write
push fname             ; Filename
call esi
add esp, 12            ; Clean up 3 args

; Check result
cmp eax, 0
jl write_error

push ok_msg
call edi
add esp, 4

xor eax, eax
pop ebp
ret

write_error:
push err_msg
call edi
add esp, 4
mov eax, 1
pop ebp
ret

fname:   db "OUTPUT.TXT", 0
data:    db "Written by an ASM program!", 10, 0
ok_msg:  db "File written successfully!", 10, 0
err_msg: db "Error writing file!", 10, 0
```

**After running:** Verify the file was written:
```
READ OUTPUT.TXT
```

---

## Example 16: Fizz Buzz (1-20)

The classic programming challenge, implemented in pure assembly.

```asm
; fizz.asm - FizzBuzz from 1 to 20
;
; What it does:
;   For each number 1-20:
;     - Divisible by 15 -> print "FizzBuzz"
;     - Divisible by 3  -> print "Fizz"
;     - Divisible by 5  -> print "Buzz"
;     - Otherwise       -> print the number
;
; Since we don't have DIV, we use counters
; to track multiples of 3 and 5.

push ebp
mov ebp, esp

mov eax, [ebp+8]
mov edi, [eax]          ; print_string
mov esi, [eax+8]        ; print_number
mov ebx, [eax+4]        ; print_char

; We'll track:
;   ECX = current number (1 to 20)
;   EDX = counter mod 3 (counts 1,2,3,1,2,3,...)
;   We'll use the stack to track mod 5

mov ecx, 1              ; Current number
; We manually track mod3 and mod5

fizz_loop:
    push ecx            ; Save current number

    ; --- Check divisible by 15 (both 3 and 5) ---
    ; We compute mod3 and mod5 using repeated subtraction
    ; Copy ECX to EAX for mod3 check
    mov eax, ecx

    ; Compute EAX mod 3 using repeated subtraction
    mod3:
        cmp eax, 3
        jl mod3_done
        sub eax, 3
        jmp mod3

    mod3_done:
    ; EAX = number mod 3
    push eax            ; Save mod3 result

    ; Compute mod 5
    mov eax, ecx        ; Reload number
    mod5:
        cmp eax, 5
        jl mod5_done
        sub eax, 5
        jmp mod5

    mod5_done:
    ; EAX = number mod 5
    ; Stack: [mod3_result]

    pop edx             ; EDX = mod3 result
    ; EAX = mod5 result

    ; Check both == 0 (FizzBuzz)
    cmp edx, 0
    jne check_fizz
    cmp eax, 0
    jne check_fizz

    ; FizzBuzz!
    push ecx
    push fb_msg
    call edi
    add esp, 4
    pop ecx
    jmp next_num

check_fizz:
    ; Re-check mod3 (EDX still has it)
    cmp edx, 0
    jne check_buzz

    ; Fizz!
    push ecx
    push f_msg
    call edi
    add esp, 4
    pop ecx
    jmp next_num

check_buzz:
    cmp eax, 0
    jne print_num

    ; Buzz!
    push ecx
    push b_msg
    call edi
    add esp, 4
    pop ecx
    jmp next_num

print_num:
    ; Print the number itself
    push ecx
    push ecx            ; number to print
    call esi            ; print_number
    add esp, 4
    pop ecx

next_num:
    ; Print space separator
    push ecx
    push 32
    call ebx
    add esp, 4
    pop ecx

    pop ecx             ; Restore counter (from fizz_loop push)
    inc ecx
    cmp ecx, 21
    jl fizz_loop

; Final newline
push 10
call ebx
add esp, 4

xor eax, eax
pop ebp
ret

f_msg:  db "Fizz", 0
b_msg:  db "Buzz", 0
fb_msg: db "FizzBuzz", 0
```

**Output:**
```
1 2 Fizz 4 Buzz Fizz 7 8 Fizz Buzz 11 Fizz 13 14 FizzBuzz 16 17 Fizz 19 Buzz
```

**Key lesson:** Without `DIV`, we compute modulo using repeated subtraction -- subtracting the divisor until the value is less than it. The remainder is our modulo result.

---

## Common Patterns Reference

### Pattern: Program Setup (use in every program)

```asm
push ebp
mov ebp, esp
mov eax, [ebp+8]       ; Get KernelAPI pointer
; ... your code ...
xor eax, eax            ; Return 0
pop ebp
ret
```

### Pattern: Save/Restore Around Function Calls

```asm
push ecx                ; Save any scratch registers you need
push arg                ; Push function argument
call function_ptr       ; Make the call
add esp, 4              ; Clean up arguments (4 bytes per arg)
pop ecx                 ; Restore your register
```

### Pattern: Counting Loop

```asm
mov ecx, START_VALUE
loop_label:
    ; ... loop body (save/restore ECX around calls) ...
    inc ecx              ; or DEC for counting down
    cmp ecx, END_VALUE
    jl loop_label        ; JL = less than, JG = greater than
```

### Pattern: Walk a String Byte by Byte

```asm
push my_string
pop edx                 ; EDX = pointer to string
next:
    mov eax, [edx]      ; Load bytes
    and eax, 0xFF       ; Mask to single byte
    cmp eax, 0          ; Null terminator?
    je done
    ; ... process character in EAX ...
    inc edx             ; Advance pointer
    jmp next
done:
```

### Pattern: Modulo Without DIV

```asm
; Compute EAX = EAX mod N (using repeated subtraction)
mod_loop:
    cmp eax, N
    jl mod_done
    sub eax, N
    jmp mod_loop
mod_done:
; EAX now contains the remainder
```

---

## KernelAPI Cheat Sheet

Quick reference for all available API functions and how to load them:

```asm
mov eax, [ebp+8]        ; EAX = KernelAPI pointer (always do this first)

; Load any function you need into a preserved register (EDI, ESI, EBX):
mov edi, [eax]           ; print_string(char*)          offset +0
mov edi, [eax+4]         ; print_char(char)             offset +4
mov edi, [eax+8]         ; print_number(unsigned int)   offset +8
mov edi, [eax+12]        ; print_hex(unsigned int)      offset +12
mov edi, [eax+16]        ; cls(void)                    offset +16
mov edi, [eax+20]        ; beep(freq, duration_ms)      offset +20
mov edi, [eax+24]        ; set_color(attr_byte)         offset +24
mov edi, [eax+28]        ; read_file(name, buf, max)    offset +28
mov edi, [eax+32]        ; write_file(name, data, size) offset +32
mov edi, [eax+36]        ; get_file_size(name)          offset +36
mov edi, [eax+40]        ; kmalloc(size)                offset +40
mov edi, [eax+44]        ; kfree(ptr)                   offset +44
```

### Argument counts per function:

| Function | Args | Push Order (right-to-left) | Clean Up |
|----------|------|---------------------------|----------|
| `print_string` | 1 | push str_addr | `add esp, 4` |
| `print_char` | 1 | push char_value | `add esp, 4` |
| `print_number` | 1 | push number | `add esp, 4` |
| `print_hex` | 1 | push number | `add esp, 4` |
| `cls` | 0 | (none) | (none) |
| `beep` | 2 | push duration, push freq | `add esp, 8` |
| `set_color` | 1 | push attr_byte | `add esp, 4` |
| `read_file` | 3 | push max, push buf, push name | `add esp, 12` |
| `write_file` | 3 | push size, push data, push name | `add esp, 12` |
| `get_file_size` | 1 | push name | `add esp, 4` |
| `kmalloc` | 1 | push size | `add esp, 4` |
| `kfree` | 1 | push ptr | `add esp, 4` |

---

## Debugging Tips

### Program crashes immediately
- **Check your prologue/epilogue.** Every program must start with `push ebp` / `mov ebp, esp` and end with `pop ebp` / `ret`.
- **Check stack cleanup.** Every `push` for a function argument needs a matching `add esp, N` after the call.

### Nothing prints
- **Are you loading the right API offset?** `print_string` is `[eax+0]`, not `[eax+4]`.
- **Is your string null-terminated?** Every string must end with `, 0` in the `DB` directive.

### Garbage output
- **Forgot to save/restore ECX in a loop?** Function calls can clobber EAX, ECX, and EDX. Always save them before calls.
- **Printing a number as a character?** `print_char` expects an ASCII code (65 = 'A'), while `print_number` expects an integer (65 prints "65").

### Assembler reports "Unknown instruction"
- Check the [supported instructions list](ASSEMBLER_REF.md#supported-instructions). The built-in assembler doesn't support MUL, DIV, SHL, SHR, or LOOP.
- Workarounds: use repeated addition for multiplication, repeated subtraction for division, and manual counter loops instead of LOOP.

### Wrong number printed
- Remember arguments are pushed **right-to-left**. For `beep(freq, ms)`, push `ms` first, then `freq`.
- Stack cleanup must match: 1 arg = `add esp, 4`, 2 args = `add esp, 8`, 3 args = `add esp, 12`.
