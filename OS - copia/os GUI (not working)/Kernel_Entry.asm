START:
[bits 32]

; Zero .bss section before calling C code
; .bss contains uninitialized globals that must be zeroed
; Note: Linker symbols _edata and _end may not be available in PE format
; So we'll do minimal zeroing here and let C code handle the rest
; For now, just ensure critical arrays are zeroed in C code

[extern _start]
	call _start
	jmp $