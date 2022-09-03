#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

#define IDT_DESC_CNT 0x81
#define PIC_M_CTRL 0x20
#define PIC_M_DATA 0x21
#define PIC_S_CTRL 0xa0
#define PIC_S_DATA 0xa1

#define EFLAGS_IF 0x00000200
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g"(EFLAG_VAR))

extern uint32_t syscall_handler(void);

struct gate_desc {
	uint16_t func_offset_low_word;
	uint16_t selector;
	uint8_t dcount;

	uint8_t attribute;
	uint16_t func_offset_high_word;
};

static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);
static struct gate_desc idt[IDT_DESC_CNT];
static void pic_init(void);
static void general_intr_handler(uint8_t vec_nr);

char* intr_name[IDT_DESC_CNT];
intr_handler idt_table[IDT_DESC_CNT];
extern intr_handler intr_entry_table[IDT_DESC_CNT];

/* initialize 8259A */
static void pic_init(void) {
	put_str("pic_init start.\n");
	//main
	outb (PIC_M_CTRL, 0x11);	//ICW1
	outb (PIC_M_DATA, 0x20);	//ICW2: idt offset
	outb (PIC_M_DATA, 0x04);	//ICW3
	outb (PIC_M_DATA, 0x01);	//ICW4
	//sub
	outb (PIC_S_CTRL, 0x11);
	outb (PIC_S_DATA, 0x28);
	outb (PIC_S_DATA, 0x02);
	outb (PIC_S_DATA, 0x01);
	/*Main 8259A:
	 *IRQ0 timer
	 *IRQ1 keyboard
	 *IRQ2 cascade to sub 8259A
	 *IRQ3 COM2
	 *IRQ4 COM1
	 *IRQ5 LPT2
	 *IRQ6 floppy disk
	 *IRQ7 LPT1
	 *
	 *Sub 8259A:
	 *IRQ8 RCT
	 *IRQ9 cascaded IRQ2
	 *IRQ10 ne2k
	 *IRQ11 reserved
	 *IRQ12 PS/2 mouse
	 *IRQ13 FPU abnormal
	 *IRQ14 disk
	 *IRQ15 reserved
	 *
	 *1011 1011  1111 1000
	 */
	outb (PIC_M_DATA, 0xf8);//intr OCW1
	outb (PIC_S_DATA, 0xbb);//OCW1

	put_str("pic_init done.\n");
}

/* create id */
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
	p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
	p_gdesc->selector = SELECTOR_K_CODE;
	p_gdesc->dcount = 0;
	p_gdesc->attribute = attr;
	p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

/* initialize idt */
static void idt_desc_init(void) {
	put_str("idt_desc_init start.\n");
	int lastIndex = IDT_DESC_CNT - 1;
	for(int i = 0; i < IDT_DESC_CNT; ++i) {
		make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
	}
	make_idt_desc(&idt[lastIndex], IDT_DESC_ATTR_DPL3, syscall_handler);
	put_str("idt_desc_init done.\n");
}

static void general_intr_handler(uint8_t vec_nr) {
	if(vec_nr == 0x27 || vec_nr == 0x2f) {
		return;
	}
	set_cursor(0);
	int cursor_pos = 0;
	while(cursor_pos < 320) {
		put_char(' ');
		++cursor_pos;
	}

	set_cursor(0);
	put_str("!!!!!!!   excetion message begin   !!!!!!!\n");
	set_cursor(88);
	put_str(intr_name[vec_nr]);
	if(vec_nr == 14) {
		int page_fault_vaddr = 0;
		asm ("movl %%cr2, %0" : "=r" (page_fault_vaddr));
		put_str("\npage fault addr is ");put_int(page_fault_vaddr);
	}
	put_str("\n!!!!!!!   excetion message end     !!!!!!!");
	while(1);
}

static void exception_init(void) {
	for(int i = 0; i < IDT_DESC_CNT; ++i) {
		idt_table[i] = general_intr_handler;
		intr_name[i] = "unknown";
	}
	intr_name[0] = "#DE Divide Error";
	intr_name[1] = "#DB Debug Exception";
	intr_name[2] = "NMI Interrupt";
	intr_name[3] = "#BP Breakpoint Exception";
	intr_name[4] = "#OF Overflow Exception";
	intr_name[5] = "#BR BOUND Range Exceeded Exception";
	intr_name[6] = "#UD Invalid Opcode Exception";
	intr_name[7] = "#NM Device Not Available Exception";
	intr_name[8] = "#DF Double Fault Exception";
	intr_name[9] = "Coprocessor Segment Overrun";
	intr_name[10] = "#TS Invalid TSS Exception";
	intr_name[11] = "#NP Segment Not Present";
	intr_name[12] = "#SS Stack Fault Exception";
	intr_name[13] = "#GP General Protection Exception";
	intr_name[14] = "#PF Page-Fault Exception";
	// intr_name[15] = "#";
	intr_name[16] = "#MF x87 FPU Floating-Point Error";
	intr_name[17] = "#AC Alignment Check Exception";
	intr_name[18] = "#MC Machine-Check Exception";
	intr_name[19] = "#XF SIMD Floating-Point Exception";
}


void register_handler(uint8_t vector_no, intr_handler function) {
	idt_table[vector_no] = function;
}

/* initialize */
void idt_init() {
	put_str("idt_init start.\n");
	idt_desc_init();
	exception_init();
	pic_init();
	
	uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)(uint32_t)idt << 16));
	asm volatile("lidt %0" : : "m" (idt_operand));
	put_str("idt_init done.\n");
}

enum intr_status intr_enable() {
	enum intr_status old_status;
	if(INTR_ON == intr_get_status()) {
		old_status = INTR_ON;
		return old_status;
	} else {
		old_status = INTR_OFF;
		asm volatile("sti");
		return old_status;
	}
}

enum intr_status intr_disable() {
	enum intr_status old_status;
	if(INTR_ON == intr_get_status()) {
		old_status = INTR_ON;
		asm volatile("cli":::"memory");
		return old_status;
	} else {
		old_status = INTR_OFF;
		return old_status;
	}
}

enum intr_status intr_set_status(enum intr_status set_status) {
	return (set_status == INTR_ON) ? intr_enable() : intr_disable();
}

enum intr_status intr_get_status() {
	uint32_t eflags = 0;
	GET_EFLAGS(eflags);
	return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}
