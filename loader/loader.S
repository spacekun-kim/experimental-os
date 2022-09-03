%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
LOADER_STACK_TOP equ LOADER_BASE_ADDR

;construct gdt
;gdt[0]
GDT_BASE:
					dd	0x00000000
					dd	0x00000000
;gdt[1]
CODE_DESC:
					dd	0x0000FFFF
					dd	DESC_CODE_HIGH4
;gdt[2]
DATA_STACK_DESC:
					dd	0x0000FFFF
					dd	DESC_DATA_HIGH4
;gdt[3]
VIDEO_DESC:
					dd	0x80000007;4k=0x7
					dd	DESC_VIDEO_HIGH4
GDT_SIZE  equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1
times 60 dq 0

;construct selector
SELECTOR_CODE  equ (0x0001 << 3) + TI_GDT + RPL0
SELECTOR_DATA  equ (0x0002 << 3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

;total memory ptr at 0x200 offset
total_mem_bytes dd 0

;construct gdt_ptr to use lgdt
gdt_ptr dw GDT_LIMIT
				dd GDT_BASE

;align loader_start at 0x300 offset
;total_mem_bytes4 + gdt_ptr6 + ards_buf244 + ards_nr2 = 0x100 
ards_buf times 244 db 0
ards_nr dw 0

loader_start:
;get memory
;int 15 eax = 0x0000e820, edx = 0x534d4150('SMAP'), ebx = 0
;output:	CF=0/1(success/error), eax = edx,	es:di=address of result(ards structure), ecx = size, ebx = next 
	xor ebx, ebx
	mov eax, 0x0000e820
	mov edx, 0x534d4150
	mov di, ards_buf
.e820_mem_get_loop:
	mov eax, 0x0000e820
	int 0x15
	jc .e820_failed_so_try_e801
	add di,cx
	inc word [ards_nr]
	cmp ebx, 0
	jnz .e820_mem_get_loop
;all ards got, find max memory address, save at edx
	mov cx, [ards_nr]
	mov ebx, ards_buf
	xor edx, edx
.find_max_mem_area:
	mov eax, [ebx]		;base_low
	add eax, [ebx + 8];length_low
	add ebx, 20
	cmp edx, eax
	jge .next_ards
	mov edx, eax
.next_ards:
	loop .find_max_mem_area
	jmp .mem_get_ok

;int 15 eax = 0x0000e801
;ax=cx:lower than 16MB situation /KB
;bx=dx:16MB ~ 4GB situation	/64KB
.e820_failed_so_try_e801:
	mov eax, 0x0000e801
	int 0x15
	jc .e801_failed_so_try_88
	and eax, 0x0000FFFF
	shl eax, 10
	add eax, 0x100000
	shl edx, 16
	add edx, eax
	jmp .mem_get_ok

;int 15 eax = 0x88
.e801_failed_so_try_88:
	mov ah, 0x88
	int 0x15
	jc .error_hlt
	and eax, 0x0000FFFF
	mov cx, 0x400
	mul cx
	shl edx, 16
	or edx, eax
	add edx, 0x100000

.mem_get_ok:
	mov [total_mem_bytes], edx
.error_hlt:
;-------------get in to PE---------------
;1.open A20
	in al, 0x92
	or al, 0000_0010B
	out 0x92, al
;2.load gdt
	lgdt [gdt_ptr]
;3.set PE at CR0[0]
	mov eax, cr0
	or eax, 0x00000001
	mov cr0, eax

;refresh flow
	jmp dword SELECTOR_CODE: p_mode_start

[bits 32]
p_mode_start:
	mov ax, SELECTOR_DATA
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov esp, LOADER_STACK_TOP
	mov ax, SELECTOR_VIDEO
	mov gs, ax

	mov byte [gs:160], 'P'
	mov byte [gs:161], 0xA4

;-----load kernel from ROM to RAM----------
	mov eax, KERNEL_START_SECTOR
	mov ebx, KERNEL_BIN_BASE_ADDR
	mov ecx, 200

;eax:start sector, ebx:RAM addr, ecx:sector count
	call rd_disk_m_32


;-------------create PDE&PTE----------------
call setup_page
;---------------re-mapping-----------------
sgdt [gdt_ptr]

mov ebx, [gdt_ptr + 2]
or dword [ebx + 0x18 + 4], VIRTUAL_KERNEL_POS 

add dword [gdt_ptr + 2], VIRTUAL_KERNEL_POS
add esp, VIRTUAL_KERNEL_POS

;--------------set PDT pos on CR3-------------
mov eax, PAGE_DIR_TABLE_POS
mov cr3, eax

;--------------set PG of CR0-----------------
mov eax, cr0
or eax, 0x80000000
mov cr0, eax

;---------------reload gdt-----------------
lgdt [gdt_ptr]

mov byte [gs:320], 'V'
mov byte [gs:321], 0xA4

;---------------refresh flow----------------
jmp SELECTOR_CODE:enter_kernel
enter_kernel:
	call kernel_init
	mov esp, 0xc009f000
	mov byte [gs:480], 'K'
	mov byte [gs:481], 0xA4

	jmp KERNEL_ENTRY_POINT

;-------------------
jmp $
;-------------------



;--------------create PDT&PTE-----------------
;PAGE_DIR_TABLE_POS = 0x100000
;PG_US_U | PG_RW_W | PG_P = 0x7
;physical memory
;|PT[1023]|0x1ff000~0x1fffff
;|   .    |.
;|   .    |.
;|   .    |.
;|PT[1]   |0x102000~0x102fff------[1]|...      |
;|PT[0]   |0x101000~0x101fff------[0]|PTE[1023]|
;																		 |  .      |
;																		 |  .      |
;																		 |  .      |
;																		 |PTE[255] |   =   [0x0ff007]
;																		 |  .      |
;																		 |  .      |   =   [0x0..007]
;																		 |  .      |			(..=0x01~0xfe)
;																		 |PTE[0]   |   =   [0x000007]
;
;|PD      |0x100000~0x100fff---------|PDE[1023]|   =   [0x100007]
;																		 |  .      |
;																		 |  .      |	 =   [0x1..007]
;																		 |  .      |			(..=0x02~0xfe)
;																		 |PDE[768] |   =   [0x101007]
;																		 |  .      |
;																		 |  .      |
;																		 |  .      |
;																		 |  .      |
;																		 |PDE[2]   |
;																		 |PDE[1]   |
;																		 |PDE[0]   |   =   [0x101007]


setup_page:
;clear RAM
	mov ecx, 4096
	mov esi, 0
.clear_page_dir:
	mov byte [PAGE_DIR_TABLE_POS + esi], 0
	inc esi
	loop .clear_page_dir

.create_pde:
	mov eax, PAGE_DIR_TABLE_POS
	add eax, 0x1000	;first pt
	mov ebx, eax
	or eax, PG_US_U | PG_RW_W | PG_P
	;prepare for kernel space
	mov [PAGE_DIR_TABLE_POS + 0x000], eax
	mov [PAGE_DIR_TABLE_POS + 0xc00], eax
	
	sub eax, 0x1000	;pde
	mov [PAGE_DIR_TABLE_POS + 0x1000 - 0x4], eax;last pde goes to pd

;create pte
	mov ecx, 256;lower 1M RAM
	mov esi, 0
	mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
	mov [ebx + esi * 4], edx
	add edx, 0x1000
	inc esi
	loop .create_pte

;create pde
	mov eax, PAGE_DIR_TABLE_POS
	add eax, 0x2000
	or eax, PG_US_U | PG_RW_W | PG_P
	mov ebx, PAGE_DIR_TABLE_POS
	mov ecx, 254
	mov esi, 769
.create_kernel_pde:
	mov [ebx + esi * 4], eax
	inc esi
	add eax, 0x1000
	loop .create_kernel_pde
ret

;------------------get kernel from mirror-------------
;0xde0
kernel_init:
	xor eax, eax	;segment start addr
	xor ebx, ebx	;header table addr
	xor ecx, ecx	;program header num
	xor edx, edx	;program header size

	mov dx, [KERNEL_BIN_BASE_ADDR + 42]
	;42: offset of e_phentsize which is program header size
	mov ebx, [KERNEL_BIN_BASE_ADDR + 28]
	;28: offset of e_phoff which is the offset of the first program header

	add ebx, KERNEL_BIN_BASE_ADDR
	mov cx, [KERNEL_BIN_BASE_ADDR + 44]
	;44: offset of e_phnum which is program header size

.each_segment:
	;skip null segment
	cmp byte [ebx + 0], PT_NULL
	;0: offset of p_type. if p_type == 0, then it is a null segment
	je .PTNULL

	push dword [ebx + 16]
	;16: offset of p_filesz
	mov eax, [ebx + 4]
	;4: offset of p_offset
	add eax, KERNEL_BIN_BASE_ADDR	
	push eax
	push dword [ebx + 8]
	;8: offset of p_vaddr

	call mem_cpy	;copy segment
	add esp, 12	;clear stack
.PTNULL:
	add ebx, edx	;get next program header
	loop .each_segment
ret


;-------------mem_cpy(dst, src, size)-------------
;-------------mem_cpy([+8],[base+4],[+16])-----------------
;0xe33
mem_cpy:
	;movs direction
	cld
	;backup
	push ebp
	mov ebp, esp
	push ecx

	;copy from esi to edi, repeat ecx times
	mov edi, [ebp + 8]
	mov esi, [ebp + 12]
	mov ecx, [ebp + 16]
	rep movsb

	;recover
	pop ecx
	pop ebp
ret

;-------read from ROM to RAM(32 version)--------
rd_disk_m_32:
;-----------------------------------------------
	;backup
	mov esi, eax
	mov di, cx
	
	;set sector count(primary as 0x1f2) to read
	mov dx, 0x1f2
	mov al, cl
	out dx, al

	mov eax, esi
	;set LBA address(0-7:0x1f3, 8-15:0x1f4, 16-32:0x1f5, 28-35:0x1f6)
	mov cl, 8
	
	mov dx, 0x1f3
	out dx, al

	shr eax, cl
	mov dx, 0x1f4
	out dx, al
	
	shr eax, cl
	mov dx, 0x1f5
	out dx, al

	shr eax, cl
	and al, 0x0f
	or ax, 0xe0
	mov dx, 0x1f6
	out dx, al

	;set comman to 0x1f7
	mov dx, 0x1f7
	mov al, 0x20
	out dx, al

	;check ROM state at 0x1f7, wait until ready
	.not_ready:
		nop	;sleep
		in al,dx
		and al, 0x88
		cmp al, 0x08
		jnz .not_ready

	;read from 0x1f0
	mov ax, di
	mov dx, 256
	mul dx
	mov cx ,ax
	mov dx, 0x1f0
	.go_on_read:
		in ax, dx
		mov [ebx], ax
		add ebx, 2
	loop .go_on_read
ret
