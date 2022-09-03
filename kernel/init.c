#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "memory.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"
#include "cmd_hash.h"
#include "ne2000.h"
#include "ksoftirqd.h"

struct lock init_done;

void init_all() {
	put_str("init_all start.\n");
	idt_init();
	mem_init();
	thread_init();
	ksoftirqd_init();
	timer_init();	
	console_init();
	keyboard_init();
	ne2000_init();
	tss_init();
	syscall_init();
	intr_enable();
	ide_init();
	filesys_init();
	cmd_list_init();
	put_str("init_all done.\n");
}
