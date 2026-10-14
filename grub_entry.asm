[bits 32]

extern _start
global _grub_entry

section .multiboot
align 4
MULTIBOOT_MAGIC equ 0x1BADB002
MULTIBOOT_FLAGS equ 0x00000003
MULTIBOOT_CHECKSUM equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

	dd MULTIBOOT_MAGIC
	dd MULTIBOOT_FLAGS
	dd MULTIBOOT_CHECKSUM

section .text
_grub_entry:
	cli
	lgdt [gdt_desc]

	mov eax, cr0
	or eax, 1
	mov cr0, eax

	jmp CODE_SEG:.reload_segments

.reload_segments:
	mov ax, DATA_SEG
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov esp, stack_top
	mov ebp, esp

	; Visible marker before entering C.
	mov byte [0xb8000], 'G'
	mov byte [0xb8001], 0x2f

	call _start

.hang:
	hlt
	jmp .hang

align 8
gdt_start:
	dq 0

gdt_code:
	dw 0xffff
	dw 0
	db 0
	db 10011010b
	db 11001111b
	db 0

gdt_data:
	dw 0xffff
	dw 0
	db 0
	db 10010010b
	db 11001111b
	db 0

gdt_end:

gdt_desc:
	dw gdt_end - gdt_start - 1
	dd gdt_start

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

section .bss
align 16
stack_bottom:
	resb 16384
stack_top:
