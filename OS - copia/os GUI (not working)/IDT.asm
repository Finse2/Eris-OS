extern _idt
extern _isr1_Handler
extern _isr12_Handler
extern _isr_default_Handler
extern _isr_exc_default_Handler
extern _isr_gp_Handler
extern _isr_pf_Handler
extern _isr_df_Handler
global _isr1
global _isr12
global _isr_default
global _isr_exc_default
global _isr_gp
global _isr_pf
global _isr_df
global _loadIdt

idtDesc:
	dw 2047  ; IDT limit = size - 1 (256 entries * 8 bytes = 2048, limit = 2047)
	dd _idt

_isr1:
	; CRITICAL: Print debug byte IMMEDIATELY (before any stack operations)
	; This helps diagnose if keyboard interrupt is firing
	push eax
	push edx
	mov al, 'K'
	mov dx, 0xE9
	out dx, al
	pop edx
	pop eax
	
	; IRQ 1 (Keyboard) - no error code pushed by CPU
	; CPU stack: [EIP] [CS] [EFLAGS]
	; Save CPU state (like example interrupt handlers)
	pusha  ; Save all general purpose registers
	mov ax, ds  ; Save data segment
	push eax
	mov ax, 0x10  ; Kernel data segment selector (from GDT: DATA_SEG = 0x10)
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	; Call C handler
	call _isr1_Handler
	
	; Restore CPU state
	pop eax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	popa
	iretd

_isr12:
	; CRITICAL: Print debug byte IMMEDIATELY (before any stack operations)
	; This helps diagnose if mouse interrupt is firing
	push eax
	push edx
	mov al, 'M'
	mov dx, 0xE9
	out dx, al
	pop edx
	pop eax
	
	; IRQ 12 (Mouse) - no error code pushed by CPU
	; CPU stack: [EIP] [CS] [EFLAGS]
	; Save CPU state
	pusha
	mov ax, ds
	push eax
	mov ax, 0x10  ; Kernel data segment selector (from GDT: DATA_SEG = 0x10)
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	; Call C handler
	call _isr12_Handler
	
	; Restore CPU state
	pop eax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	popa
	iretd

_isr_gp:
	; #GP (General Protection Fault) pushes an error code
	; CPU stack: [error_code] [EIP] [CS] [EFLAGS]
	; We must pop the error code before returning
	
	; Save CPU state
	pusha
	mov ax, ds
	push eax
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	call _isr_gp_Handler
	
	; Note: gp handler halts, so this won't execute
	; But if it returns, we need to restore state correctly
	pop eax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	popa
	
	; Pop the error code that CPU pushed (exceptions with error codes)
	add esp, 4  ; Remove error code from stack
	
	iretd

_isr_pf:
	; #PF (Page Fault) pushes an error code
	; CPU stack: [error_code] [EIP] [CS] [EFLAGS]
	; We must pop the error code before returning
	
	; Save CPU state
	pusha
	mov ax, ds
	push eax
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	call _isr_pf_Handler
	
	; Restore CPU state (handler may continue or halt)
	pop eax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	popa
	
	; Pop the error code that CPU pushed (exceptions with error codes)
	add esp, 4  ; Remove error code from stack
	
	iretd

_isr_df:
	; #DF (Double Fault) pushes an error code (always 0, but still pushed)
	; CPU stack: [error_code] [EIP] [CS] [EFLAGS]
	; We must pop the error code before returning (even though handler halts)
	
	; Save CPU state
	pusha
	mov ax, ds
	push eax
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	call _isr_df_Handler
	
	; Note: df handler halts, so this won't execute
	; But if it somehow returns, we need to restore state correctly
	pop eax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	popa
	
	; Pop the error code that CPU pushed (exceptions with error codes)
	add esp, 4  ; Remove error code from stack
	
	iretd

_isr_default:
	; CRITICAL: Print debug byte IMMEDIATELY (before any stack operations)
	; This helps diagnose if the crash is in the ISR stub itself
	push eax
	push edx
	mov al, 'D'
	mov dx, 0xE9
	out dx, al
	pop edx
	pop eax
	
	; Save CPU state
	pusha
	mov ax, ds
	push eax
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	call _isr_default_Handler
	
	; Restore CPU state
	pop eax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	popa
	iretd

_isr_exc_default:
	; Default handler for unexpected EXCEPTIONS (not IRQs)
	; Save CPU state
	pusha
	mov ax, ds
	push eax
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	call _isr_exc_default_Handler
	
	; If it ever returns, restore CPU state and return
	pop eax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	popa
	iretd

_loadIdt:
	; Debug marker at start of loadIdt
	push eax
	push edx
	mov al, 'S'
	mov dx, 0xE9
	out dx, al
	pop edx
	pop eax
	
	lidt[idtDesc]
	
	; Debug marker after lidt
	push eax
	push edx
	mov al, 'T'
	mov dx, 0xE9
	out dx, al
	pop edx
	pop eax
	
	; DO NOT enable interrupts here - caller should do it after everything is ready
	; This prevents crashes if an interrupt fires before handlers/PIC are fully configured
	ret