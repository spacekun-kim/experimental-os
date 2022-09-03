[bits 32]
extern main
extern exit
section .text
global _start
_start:
	push ebx ;push argv
	push ecx ;push argc
	call main

	push eax
	call exit
