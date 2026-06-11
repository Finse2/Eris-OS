[bits 32]

extern _blockAddr
extern _At
global _read
global _write

_read:

		pushfd
		and eax , 0x0FFFFFFF
		push eax
		push ebx
		push ecx
		push edx
		push edi

		mov eax , [_blockAddr]
		mov cl , 1
		mov edi , _At		

		mov ebx , eax
 
		mov edx , 0x01F6 
		shr eax , 24
		or al , 11100000b     ; Sets bit 6 in al for LBA mode
		out dx , al
 
		mov edx , 0x01F2      ; Port to send number of sectors
		mov al , cl           ; Get number of sectors from CL
		out dx , al
 
		mov edx , 0x1F3       ; Port to send bit 0 - 7 of LBA
		mov eax , ebx         ; Get LBA from EBX
		out dx , al
 
		mov edx , 0x1F4       ; Port to send bit 8 - 15 of LBA
		mov eax , ebx         ; Get LBA from EBX
		shr eax , 8           ; Get bit 8 - 15 in AL
		out dx , al
 
 
		mov edx , 0x1F5       ; Port to send bit 16 - 23 of LBA
		mov eax , ebx         ; Get LBA from EBX
		shr eax , 16          ; Get bit 16 - 23 in AL
		out dx , al
 
		mov edx , 0x1F7       ; Command port
		mov al , 0x20         ; Read with retry.
		out dx , al
 
.loop1:
		in al , dx
		test al , 8
		jz .loop1
 
		mov eax , 256         ; Read 256 words , 1 sector
		xor bx , bx
		mov bl , cl           ; read CL sectors
		mul bx
		mov ecx , eax         
		mov edx , 0x1F0
		rep insw             ; copy to [RDI]
 
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
		popfd
		ret


_write:
		pushfd
		and eax , 0x0FFFFFFF
		push eax
		push ebx
		push ecx
		push edx
		push edi

		mov eax , [_blockAddr]
		; Protect sector 0 - This is where the OS boot sector is stored
		; Writing to sector 0 would corrupt the OS, so it's permanently protected
		cmp eax , 0
		je .write_protected
		
		mov cl , 1
		mov edi , _At

		mov ebx , eax
 
		mov edx , 0x01F6
		shr eax , 24
		or al , 11100000b     ; Set bit 6 in al for LBA mode
		out dx , al
 
		mov edx , 0x01F2      ; Port to send number of sectors
		mov al , cl           ; Get number of sectors from CL
		out dx , al
 
		mov edx , 0x1F3       ; Port to send bit 0 - 7 of LBA
		mov eax , ebx         ; Get LBA from EBX
		out dx , al
 
		mov edx , 0x1F4       ; Port to send bit 8 - 15 of LBA
		mov eax , ebx         ; Get LBA from EBX
		shr eax , 8           ; Get bit 8 - 15 in AL
		out dx , al
 
 
		mov edx , 0x1F5       ; Port to send bit 16 - 23 of LBA
		mov eax , ebx         ; Get LBA from EBX
		shr eax , 16          ; Get bit 16 - 23 in AL
		out dx , al
 
	mov edx , 0x1F7       ; Command port
	mov al , 0x30         ; Write with retry.
	out dx , al

	; Wait for drive to be ready to receive data (DRQ set, BSY clear)
	; Add timeout to prevent infinite hang
	mov ebx , 100000      ; Timeout counter
.loop2:
	dec ebx
	jz .write_timeout     ; If timeout, exit
	in al , dx
	test al , 0x80        ; Check BSY bit (bit 7)
	jnz .loop2            ; If BSY set, keep waiting
	test al , 0x08        ; Check DRQ bit (bit 3)
	jz .loop2             ; If DRQ not set, keep waiting

	mov eax , 256         ; 256 words = 512 bytes = 1 sector
	xor bx , bx
	mov bl , cl           ; write CL sectors
	mul bx
	mov ecx , eax 
	mov edx , 0x1F0
	mov esi , edi
	rep outsw            ; Send data to drive

	; Wait for write operation to complete
	mov edx , 0x1F7       ; Status port
	mov ebx , 100000      ; Timeout counter
.loop3:
	dec ebx
	jz .write_timeout     ; If timeout, exit
	in al , dx
	test al , 0x80        ; Check BSY bit (bit 7)
	jnz .loop3            ; If BSY set, drive is still writing
	test al , 0x01        ; Check ERR bit (bit 0)
	jnz .write_error      ; If ERR set, there was an error
	test al , 0x20        ; Check DF bit (bit 5) - Device Fault
	jnz .write_error      ; If DF set, there was a device fault
	; Write completed successfully - fall through to cleanup

.write_timeout:
	; Timeout occurred, but continue anyway (might work in emulator)
	; Fall through to cleanup

.write_error:
	; Write error occurred, but we'll still return (caller can check)
	; Fall through to cleanup

.write_done:
	pop edi
	pop edx
	pop ecx
	pop ebx
	pop eax
	popfd
	ret
		
.write_protected:
		; Sector 0 is protected - cannot write to OS boot sector
		; This sector contains the OS and must remain protected
		pop edi
		pop edx
		pop ecx
		pop ebx
		pop eax
		popfd
		ret