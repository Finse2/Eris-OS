[bits 32]
[extern _start]

section .text

START:
	; Debug: Write 'K' to screen
	mov byte [0xb8000], 'K'
	mov byte [0xb8001], 0x4f

	call _start
	jmp $

; -------------------------------------------------
; VBE helper (Bochs/QEMU VBE ports)
; -------------------------------------------------
vbe_write_reg:
	; IN: BX=index, AX=value
	push edx
	push eax
	mov dx, 0x01CE
	mov ax, bx
	out dx, ax
	pop eax
	mov dx, 0x01CF
	out dx, ax
	pop edx
	ret

vbe_set_mode:
	; Disable
	mov bx, 0x0004
	mov ax, 0x0000
	call vbe_write_reg
	; XRES = 1024
	mov bx, 0x0001
	mov ax, 1024
	call vbe_write_reg
	; YRES = 768
	mov bx, 0x0002
	mov ax, 768
	call vbe_write_reg
	; BPP = 32
	mov bx, 0x0003
	mov ax, 32
	call vbe_write_reg
	; ENABLE | LFB
	mov bx, 0x0004
	mov ax, 0x0041
	call vbe_write_reg
	ret
