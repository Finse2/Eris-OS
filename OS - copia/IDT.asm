extern _idt
extern _isr0_Handler
extern _isr1_Handler
extern _isr11_Handler
extern _isr12_Handler
extern _isr14_Handler
global _isr0
global _isr1
global _isr11
global _isr12
global _isr14
global _loadIdt

idtDesc:
	dw 2047       ; limit = (256 entries * 8 bytes) - 1
	dd _idt

; IRQ0 - PIT Timer
_isr0:
	pushad
	call _isr0_Handler
	popad
	iretd

_isr1:
	pushad
	call _isr1_Handler
	popad
	iretd

_isr11:
	pushad
	call _isr11_Handler
	popad
	iretd

_isr12:
	pushad
	call _isr12_Handler
	popad
	iretd

; Page fault handler (exception 14)
; CPU pushes an error code on the stack before entering this handler.
; We must pop it after popad / before iretd.
_isr14:
	pushad
	call _isr14_Handler
	popad
	add esp, 4          ; Remove CPU-pushed error code
	iretd

_loadIdt:
	lidt[idtDesc]
	sti 
	ret
	
