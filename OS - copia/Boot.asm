[org 0x7c00]
[bits 16]

%include "boot_sectors.inc"

; Save boot drive
mov [boot_drive], dl

; Setup DS
xor ax, ax
mov ds, ax

; Boot marker: entered boot sector
mov ah, 0x0e
mov al, 'B'
int 0x10

; Load kernel to 0x10000 (64KB mark) - safe from bootloader
; Use ES:BX = 0x1000:0x0000 = 0x10000 physical
mov ax, 0x1000
mov es, ax
xor bx, bx              ; ES:BX = 0x1000:0x0000 = 0x10000

; Generic CHS read loop (1.44MB floppy geometry: 18 sectors/track, 2 heads)
; Start from C/H/S 0/0/2 and load SECTOR_COUNT sectors.
mov word [sectors_left], SECTOR_COUNT
mov byte [cur_cyl], 0
mov byte [cur_head], 0
mov byte [cur_sector], 2
mov word [load_seg], 0x1000
mov word [load_off], 0

read_loop:
    mov ax, [sectors_left]
    cmp ax, 0
    je read_done

    mov ax, [load_seg]
    mov es, ax
    mov bx, [load_off]
    mov ch, [cur_cyl]
    mov cl, [cur_sector]
    mov dh, [cur_head]
    mov ah, 0x02
    mov al, 1           ; Read one sector per iteration
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    ; Advance destination by 512 bytes, fixing 64KB boundary crossings.
    mov ax, [load_off]
    add ax, 512
    mov [load_off], ax
    jnc next_chs
    mov ax, [load_seg]
    add ax, 0x1000
    mov [load_seg], ax
    mov word [load_off], 0

next_chs:
    mov al, [cur_sector]
    inc al
    mov [cur_sector], al
    cmp al, 19
    jb chs_ok
    mov byte [cur_sector], 1
    mov al, [cur_head]
    inc al
    mov [cur_head], al
    cmp al, 2
    jb chs_ok
    mov byte [cur_head], 0
    inc byte [cur_cyl]
chs_ok:
    dec word [sectors_left]
    jmp read_loop

read_done:

; Boot marker: kernel fully read
mov ah, 0x0e
mov al, 'L'
int 0x10

; Print 'C' - all done
mov ah, 0x0e
mov al, 'C'
int 0x10

; Switch to protected mode
cli
lgdt [GDT_DESC]

mov eax, cr0
or eax, 1
mov cr0, eax

jmp CODE_SEG:INIT_PM

disk_error:
    mov ah, 0x0e
    mov al, 'X'
    int 0x10
    jmp $

[bits 32]

INIT_PM:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    mov ebp, 0x90000
    mov esp, ebp
    
    ; Debug 'D' in video memory
    mov byte [0xb8006], 'D'
    mov byte [0xb8007], 0x0f
    
    ; Kernel is now at 0x10000
    call 0x10000
    jmp $

; Data
boot_drive: db 0
sectors_left: dw 0
load_seg: dw 0
load_off: dw 0
cur_cyl: db 0
cur_head: db 0
cur_sector: db 0

; GDT
GDT_BEGIN:
    dd 0, 0

GDT_CODE_SEG:
    dw 0xffff, 0
    db 0, 10011010b, 11001111b, 0

GDT_DATA_SEG:
    dw 0xffff, 0
    db 0, 10010010b, 11001111b, 0

GDT_END:

GDT_DESC:
    dw GDT_END - GDT_BEGIN - 1
    dd GDT_BEGIN

CODE_SEG equ GDT_CODE_SEG - GDT_BEGIN
DATA_SEG equ GDT_DATA_SEG - GDT_BEGIN

times 510-($-$$) db 0
dw 0xaa55
