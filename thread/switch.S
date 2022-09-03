[bits 32]
section .text
global switch_to
switch_to:
	;backup
	push esi
	push edi
	push ebx
	push ebp
	;save current PCB address
	mov eax, [esp + 20]
	mov [eax], esp
	;load next PCB address
	mov eax, [esp + 24]
	mov esp, [eax]
	;recover
	pop ebp
	pop ebx
	pop edi
	pop esi
	ret
