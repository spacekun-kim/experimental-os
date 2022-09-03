[bits 32]
%define ERROR_CODE nop
%define ZERO push 0

extern idt_table

section .data
global intr_entry_table
intr_entry_table:

%macro VECTOR 2
section .text
intr%1entry:
	%2
;save context
	push ds
	push es
	push fs
	push gs
	pushad

;send EOI to main & sub	
	mov al, 0x20
	out 0xa0, al
	out 0x20, al

	push %1
	call [idt_table + %1 * 4]
	jmp intr_exit

section .data
	dd intr%1entry
%endmacro

section .text
extern check_signal
extern get_arg
extern signal_handler
extern backup_intr_stack
global intr_exit
intr_exit:
;recover context
	call check_signal
	cmp eax, 0
	jne .do_signal
	add esp, 4
	popad
	pop gs
	pop fs
	pop es
	pop ds
	add esp, 4
	iretd
.do_signal:
;recover sregs
	mov gs, [esp + 4 * 9]
	mov fs, [esp + 4 * 10]
	mov es, [esp + 4 * 11]
	mov ds, [esp + 4 * 12]
;ebx = handler function
	mov ebx, eax
;push (cs in stack)
	push dword [esp + 4 * 15]
	call check_cpl_down
	add esp, 4

	push esp
	push edx
	call backup_intr_stack
; if cpl not down, it has esp and ss in stack	
	mov ecx, 4
	mov edx, 4 * 18
	
	cmp dword [esp], 1
	je .cpl_down
;if cpl down, it has not esp or ss in stack
	mov ecx, 2
	mov edx, 4 * 16
	push 0
	push 0
.cpl_down:
	add esp, 8
.make_stack:
	mov eax, [esp + edx]
	push eax
	loop .make_stack
	
	push signal_handler_
	sub dword [esp + 3 * 4], 4
	call get_arg
	iretd

signal_handler_:
	push eax
	push ebx
	push 0
	jmp signal_handler

VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO
VECTOR 0x08, ZERO
VECTOR 0x09, ZERO
VECTOR 0x0a, ZERO
VECTOR 0x0b, ZERO
VECTOR 0x0c, ZERO
VECTOR 0x0d, ERROR_CODE
VECTOR 0x0e, ZERO
VECTOR 0x0f, ZERO
VECTOR 0x10, ZERO
VECTOR 0x11, ZERO
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO
VECTOR 0x18, ZERO
VECTOR 0x19, ZERO
VECTOR 0x1a, ZERO
VECTOR 0x1b, ZERO
VECTOR 0x1c, ZERO
VECTOR 0x1d, ZERO
VECTOR 0x1e, ERROR_CODE
VECTOR 0x1f, ZERO
VECTOR 0x20, ZERO
VECTOR 0x21, ZERO
VECTOR 0x22, ZERO
VECTOR 0x23, ZERO
VECTOR 0x24, ZERO
VECTOR 0x25, ZERO
VECTOR 0x26, ZERO
VECTOR 0x27, ZERO
VECTOR 0x28, ZERO
VECTOR 0x29, ZERO
VECTOR 0x2a, ZERO
VECTOR 0x2b, ZERO
VECTOR 0x2c, ZERO
VECTOR 0x2d, ZERO
VECTOR 0x2e, ZERO
VECTOR 0x2f, ZERO

;-------------- int 0x80 ---------------
[bits 32]
extern syscall_table
%define SYS_SIGRETURN 0
section .text
global syscall_handler
syscall_handler:
;1. save context
	push 0
	push ds
	push es
	push fs
	push gs
	pushad			;eax,ecx,edx,ebx,esp,ebp,esi,edi
	push 0x80		;keep format
	
	cmp eax, SYS_SIGRETURN
	je sys_sigreturn
;2. input paras
	push edx
	push ecx
	push ebx
;3. call function
	call [syscall_table + eax * 4]
	add esp, 12
;4. get retval into eax pos of stack
	mov [esp + 8 * 4], eax
	jmp intr_exit

;--------------sys_sigreturn------------
[bits 32]
extern event_over
global sys_sigreturn
sys_sigreturn:
	call event_over
	cmp eax, esp
	je .goto_exit
	sub eax, esp
	add esp, eax
.goto_exit:
	jmp intr_exit

;------------check_if_cpl_down----------
;if cpl down edx = 1, else edx = 0
[bits 32]
check_cpl_down:
	push eax
	push ebx
	mov eax, [esp + 12]
	mov ebx, cs
	and eax, 3
	and ebx, 3
	mov edx, 1
	cmp eax, ebx
	jg .cpl_down
	mov edx, 0
.cpl_down:
	pop eax
	pop ebx
	ret
