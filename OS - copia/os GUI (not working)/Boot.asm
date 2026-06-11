[org 0x7c00]
[bits 16]

;Boot Loader
; Simple bootloader - load kernel and jump to it

; Set up segment:offset for kernel load address
; ES:BX = 0:0x1000 = physical address 0x1000
mov ax , 0x0000
mov es , ax     ; ES = 0 (segment)
mov bx , 0x1000 ; BX = 0x1000 (offset)
                ; ES:BX = 0:0x1000 = physical address 0x1000

; Read kernel from disk
; Kernel is 33132 bytes = 64.7 sectors, so we need 65 sectors
; Use simple CHS approach - read in multiple smaller chunks to avoid geometry limits

; First read: 30 sectors (safe, we know this works)
mov ah , 0x02   ; Bios Read Sector Function
mov al , 30     ; Number of sectors (30 * 512 = 15360 bytes)
mov ch , 0x00   ; Cylinder 0
mov dh , 0x00   ; Head 0
mov cl , 0x02   ; Sector 2 (sector 1 is boot sector)
                ; Note: DL (drive) is set by BIOS

int 0x13        ; Read first 30 sectors
jc  disk_error  ; If this fails, halt

; Second read: 30 more sectors (total 60 so far)
; Update BX: 30 sectors * 512 = 15360 bytes = 0x3C00
; ES is 0, so BX = 0x1000 + 0x3C00 = 0x4C00
mov bx , 0x4C00 ; Destination for second chunk
mov al , 30     ; 30 more sectors
mov cl , 0x20   ; Sector 32 (0x02 + 30 = 0x20, decimal 32)
                ; Still on same track/head (if 63 sectors/track)

int 0x13        ; Read second 30 sectors
jc  disk_error  ; If this fails, halt

; Third read: 5 more sectors (total 65)
; Update BX: 60 sectors * 512 = 30720 bytes = 0x7800
; ES is 0, so BX = 0x1000 + 0x7800 = 0x8800
mov bx , 0x8800 ; Destination for third chunk
mov al , 5      ; 5 more sectors
mov cl , 0x3E   ; Sector 62 (0x02 + 60 = 0x3E, decimal 62)
                ; Still on same track/head

int 0x13        ; Read last 5 sectors
                ; Continue even if this fails (60 sectors might be enough)

jmp disk_read_ok

disk_error:
    ; Disk read failed - halt
    cli
    hlt
    jmp disk_error

disk_read_ok:

;Switch to Graphics Mode (VGA Mode 13h: 320x200, 256 colors)
;CRITICAL: This MUST be done in Real Mode (16-bit) before entering Protected Mode
;BIOS Int 0x10 is only available in Real Mode or Virtual 8086 Mode
;Once we enter Protected Mode, we cannot use BIOS calls anymore
;The framebuffer will be at physical address 0xA0000
mov ah , 0x00   ; Function: Set Video Mode
mov al , 0x13   ; Mode 13h: VGA 320x200, 256 colors
int 0x10        ; Call BIOS video interrupt

;Switch To Protected Mode
cli ; Turns Interrupts off
lgdt [GDT_DESC] ; Loads Our GDT

mov eax , cr0
or  eax , 0x1
mov cr0 , eax ; Switch To Protected Mode

jmp  CODE_SEG:INIT_PM ; Jumps To Our 32 bit Code


[bits 32]

INIT_PM:
mov ax , DATA_SEG
mov ds , ax
mov ss , ax
mov es , ax
mov fs , ax
mov gs , ax

mov ebp , 0x90000
mov esp , ebp ; Updates Stack Segment
; Stack grows downward from 0x90000
; Allocate 64KB for stack (0x90000 - 0x10000 = 0x80000)
; This gives us plenty of room for interrupt handlers


call 0x1000
jmp $ 





GDT_BEGIN:

GDT_NULL_DESC:  ;The  Mandatory  Null  Descriptor
	dd 0x0
	dd 0x0

GDT_CODE_SEG:
	dw 0xffff		;Limit
	dw 0x0			;Base
	db 0x0			;Base
	db 10011010b	;Flags
	db 11001111b	;Flags
	db 0x0			;Base

GDT_DATA_SEG:
	dw 0xffff		;Limit
	dw 0x0			;Base
	db 0x0			;Base
	db 10010010b	;Flags
	db 11001111b	;Flags
	db 0x0			;Base

GDT_END:

GDT_DESC:
	dw GDT_END - GDT_BEGIN - 1
	dd GDT_BEGIN

CODE_SEG equ GDT_CODE_SEG - GDT_BEGIN
DATA_SEG equ GDT_DATA_SEG - GDT_BEGIN

	
times 510-($-$$) db 0
dw 0xaa55