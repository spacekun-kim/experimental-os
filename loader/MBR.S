%include "boot.inc"

;--------------------
;initialize registers
;--------------------
SECTION MBR vstart=0x7c00
	mov ax,cs
	mov ds,ax
	mov es,ax
	mov ss,ax
	mov fs,ax
	mov sp,0x7c00
	mov ax,0xb800
	mov gs,ax

;--------------------
;clear screen by using AH = 0x06
;AL = scroll lines(0 means all)
;BH = scroll lines properties
;(CL,CH) = left up axis
;(DL,DH) = right bottom axis
;no return
;--------------------
	mov ax, 0x0600
	mov bx, 0x0700
	mov cx, 0x0000
	mov dx, 0x184f	;25*80
	int 0x10

;--------------------
;get current curser pos
;--------------------
	mov ah,3
	mov bh,0
	int 0x10

;print message to screen
;	mov ax, message
;	mov bp, ax
;
;	mov bx, 0x2
;	int 0x10
;bh saves page number
;bl saves property of characters and background(0x2 means black bg, green word)

;--------------------
;send characters to video memory
;--------------------
	mov byte [gs:0x00],'J'
	mov byte [gs:0x01],0xA4

	mov byte [gs:0x02],'y'
	mov byte [gs:0x03],0xA4

	mov byte [gs:0x04],'j'
	mov byte [gs:0x05],0xA4

	mov byte [gs:0x06],0x27	;'
	mov byte [gs:0x07],0xA4

	mov byte [gs:0x08],'s'
	mov byte [gs:0x09],0xA4

	mov byte [gs:0x0A],' '
	mov byte [gs:0x0B],0xA4

	mov byte [gs:0x0C],'M'
	mov byte [gs:0x0D],0xA4

	mov byte [gs:0x0E],'B'
	mov byte [gs:0x0F],0xA4

	mov byte [gs:0x10],'R'
	mov byte [gs:0x11],0xA4

;--------------------
;read loader from ROM
;--------------------
mov eax,LOADER_START_SECTOR
mov bx,LOADER_BASE_ADDR
mov cx,4
call rd_disk_m_16
jmp LOADER_BASE_ADDR + LOADER_START_OFFSET


;--------------------
;read [cx] sections from ROM address [eax] to RAM [bx]
rd_disk_m_16:
;--------------------
	;backup
	mov esi,eax
	mov di,cx

	;set sector count(primary as 0x1f2) to read
	mov dx,0x1f2
	mov al,cl
	out dx,al

	mov eax,esi

	;set LBA address(0-7:0x1f3,8-15:0x1f4,16-23:0x1f5,28-25:0x1f6)
	mov cl,8

	mov dx,0x1f3
	out dx,al

	shr eax,cl
	mov dx,0x1f4
	out dx,al
	
	shr eax,cl
	mov dx,0x1f5
	out dx,al

	shr eax,cl
	and al,0x0f
	or ax,0xe0
	mov dx,0x1f6
	out dx,al

	;set command to 0x1f7
	mov dx,0x1f7
	mov al,0x20
	out dx,al

	;check ROM state at 0x1f7, wait until ready
	.not_ready:
		nop	;sleep
		in al,dx
		and al,0x88	;10001000,3bit:ready,7bit:busy
		cmp al,0x08
		jnz .not_ready

	;read from 0x1f0
	mov ax,di
	mov dx,256
	mul dx
	mov cx,ax	;loop count
	mov dx,0x1f0
	.go_on_read:
		in ax,dx
		mov [bx],ax
		add bx,2
		loop .go_on_read
	ret
;--------------------
;freeze program
;--------------------
;jmp $

message db "Jyj's first MBR"
times 510-($-$$) db 0
db 0x55,0xaa
