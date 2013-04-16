[org 0x0]
_rst:	jmp rel main
_irq:	jmp rel $
_mabrt:	jmp rel $
_undef:	jmp rel $

main:
	mov r13, 0x10100000	; Top of DRAM+1

	mov r2, 0x12345
	call puthex

	jmp $

; TODO: [TEXTDATA] (or similar) for 32-bit literals

puthex:
	push r14
	
.loop:
	mov r0, r2
	shr r2, 4
	and r0, 0xF
	ldrbz r0, [r0 + hex]
	call putch
	test r2, r2
	jmp.nz .loop
	
	pop r14
	jmp r14

putch:
	mov r15, 0x20000000	; UART Base
	mov [r15], r0
	jmp r14

hex: dw	"0123456789abcdef"

[ALIGN 0x4000]
