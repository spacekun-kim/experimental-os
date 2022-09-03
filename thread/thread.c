#include "thread.h"
#include "global.h"
#include "string.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "process.h"
#include "sync.h"
#include "file.h"
#include "stdio.h"

/* pid control */
#define PID_MAX 1024
uint8_t pid_map[PID_MAX] = {0};
struct pid_pool {
	struct lock pid_lock;
	uint32_t size;
	int16_t pid_start;
	uint8_t* tids_in_pid;
}pid_pool;

struct task_struct* main_thread;
struct list thread_ready_list;
struct list thread_all_list;
static struct list_elem* thread_tag;

struct task_struct* idle_thread;

extern void switch_to(struct task_struct* cur, struct task_struct* next);

static void idle(void* arg UNUSED) {
	while(1) {
		thread_block(TASK_BLOCKED);
		asm volatile ("sti; hlt" : : : "memory");
	}
}

struct task_struct* running_thread(void) {
	uint32_t esp;
	asm ("mov %%esp, %0" : "=g" (esp));
	return (struct task_struct*)(esp & 0xfffff000);
}

static void kernel_thread(thread_func* function, void* func_arg) {
	intr_enable();
	function(func_arg);
}

static void pid_pool_init(void) {
	pid_pool.pid_start = 1;
	pid_pool.size = PID_MAX;
	pid_pool.tids_in_pid = pid_map;
	lock_init(&pid_pool.pid_lock);
}

static bool is_last_pid(pid_t pid) {
	int32_t bit_idx = pid - pid_pool.pid_start;
	return pid_pool.tids_in_pid[bit_idx] == 1;
}

pid_t thread2process_pid(pid_t pid, pid_t tid) {
	lock_acquire(&pid_pool.pid_lock);
	if(pid_pool.tids_in_pid[pid - pid_pool.pid_start] == 1) {
		lock_release(&pid_pool.pid_lock);
		return tid;
	}
	--pid_pool.tids_in_pid[pid - pid_pool.pid_start];
	lock_release(&pid_pool.pid_lock);
	return tid;
}

static pid_t allocate_pid(pid_t pid) {
	lock_acquire(&pid_pool.pid_lock);
	if(pid != 0) { // new pid has a parent pid
		++pid_pool.tids_in_pid[pid - pid_pool.pid_start];
		lock_release(&pid_pool.pid_lock);
		return pid;
	}
	uint32_t i;
	int32_t bit_idx = 0;
	for(i = 0; i < pid_pool.size; ++i) {
		if(pid_pool.tids_in_pid[i] == 0) {
			pid_pool.tids_in_pid[i] = 1;
			bit_idx = i;
			break;
		}
	}
	lock_release(&pid_pool.pid_lock);
	return bit_idx + pid_pool.pid_start;
}

static pid_t allocate_tid(pid_t pid) {
	lock_acquire(&pid_pool.pid_lock);
	if(pid_pool.tids_in_pid[pid - pid_pool.pid_start] == 1) {
		lock_release(&pid_pool.pid_lock);
		return pid;
	}
	uint32_t i;
	int32_t bit_idx;
	for(i = 0; i < pid_pool.size; ++i) {
		if(pid_pool.tids_in_pid[i] == 0) {
			bit_idx = i;
			pid_pool.tids_in_pid[i] = 1; // tid
			++pid_pool.tids_in_pid[pid - pid_pool.pid_start]; // pid
			break;
		}
	}
	lock_release(&pid_pool.pid_lock);
	return (bit_idx + pid_pool.pid_start);
}

static void release_pid(pid_t pid, pid_t tid) {
	lock_acquire(&pid_pool.pid_lock);
	int32_t pid_idx = pid - pid_pool.pid_start;
	int32_t tid_idx = tid - pid_pool.pid_start;

	--pid_pool.tids_in_pid[tid_idx];
	if(pid_idx != tid_idx) {
		--pid_pool.tids_in_pid[pid_idx];
	}
	lock_release(&pid_pool.pid_lock);
}

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {
	pthread->self_kstack -= sizeof(struct intr_stack);
	pthread->self_kstack -= sizeof(struct thread_stack);
	struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
	kthread_stack->eip = kernel_thread;
	kthread_stack->function = function;
	kthread_stack->func_arg = func_arg;
	kthread_stack->ebp = kthread_stack->ebx = \
	kthread_stack->esi = kthread_stack->edi = 0;
}

void init_thread(struct task_struct* pthread, char* name, int prio) {
	memset(pthread, 0, sizeof(*pthread));
	struct task_struct* cur_thread = running_thread();
	pthread->pid = allocate_pid(cur_thread->pid);
	pthread->tid = allocate_tid(pthread->pid);
	strcpy(pthread->name, name);

	if(pthread == main_thread) {
		pthread->status = TASK_RUNNING;
	} else {
		pthread->status = TASK_READY;
	}

	pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);
	pthread->priority = prio;
	pthread->ticks = prio;
	pthread->elapsed_ticks = 0;
	pthread->pgdir = cur_thread->pgdir;
	pthread->userprog_vaddr = cur_thread->userprog_vaddr;
	pthread->fd_table[0] = 0;
	pthread->fd_table[1] = 1;
	pthread->fd_table[2] = 2;
	uint8_t fd_idx = 3;
	while(fd_idx < MAX_FILES_OPEN_PER_PROC) {
		pthread->fd_table[fd_idx] = -1;
		++fd_idx;
	}
	pthread->cwd_inode_nr = 0;
	pthread->parent_pid = -1;
	pthread->stack_magic = 0x19870916;
}

struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {
	struct task_struct* thread = get_kernel_pages(1);
	init_thread(thread, name, prio);
	thread_create(thread, function, func_arg);

	ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
	ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
	
	list_append(&thread_ready_list, &thread->general_tag);
	list_append(&thread_all_list, &thread->all_list_tag);
	
	/*asm volatile ("\
			movl %0, %%esp;\
			pop %%ebp;\
			pop %%ebx;\
			pop %%edi;\
			pop %%esi;\
			ret": : "g" (thread->self_kstack) : "memory");
	*/
	return thread;
}

void schedule(void) {
	ASSERT(intr_get_status() == INTR_OFF);
	struct task_struct* cur = running_thread();
	if(cur->status == TASK_RUNNING) {
		ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
		list_append(&thread_ready_list, &cur->general_tag);
		cur->ticks = cur->priority;
		cur->status = TASK_READY;
	} else {
	//
	}
	if(list_empty(&thread_ready_list)) {
		thread_unblock(idle_thread);
	}
	thread_tag = (struct list_elem*)NULL;
	thread_tag = list_pop(&thread_ready_list);
	struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
	next->status = TASK_RUNNING;
	process_activate(next);
	switch_to(cur, next);
}

static void make_main_thread(void) {
	main_thread = running_thread();
	init_thread(main_thread, "main", 31);
	put_str("main thread PCB: 0x");
	put_int((uint32_t)main_thread);
	put_char('\n');
	ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
	list_append(&thread_all_list, &main_thread->all_list_tag);
}

void thread_yield(void) {
	struct task_struct* cur = running_thread();
	enum intr_status old_status = intr_disable();
	ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
	list_append(&thread_ready_list, &cur->general_tag);
	cur->status = TASK_READY;
	schedule();
	intr_set_status(old_status);
}

void thread_exit(struct task_struct* thread_over, bool need_schedule) {
	intr_disable();
	thread_over->status = TASK_DIED;
	
	if(elem_find(&thread_ready_list, &thread_over->general_tag)) {
		list_remove(&thread_over->general_tag);
	}
	if(is_last_pid(thread_over->pid)) {
		mfree_page(PF_KERNEL, thread_over->pgdir, 1);
	}

	list_remove(&thread_over->all_list_tag);

	if(thread_over != main_thread) {
		mfree_page(PF_KERNEL, thread_over, 1);
	}

	release_pid(thread_over->pid, thread_over->tid);

	if(need_schedule) {
		schedule();
		PANIC("thread_exit: should not be here\n");
	}
}

static bool pid_check(struct list_elem* pelem, int32_t pid) {
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
	if(pthread->pid == pid) {
		return true;
	}
	return false;
}

static bool tid_check(struct list_elem* pelem, int32_t tid) {
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
	if(pthread->tid == tid) {
		return true;
	}
	return false;
}

struct task_struct* pid2thread(int32_t pid) {
	struct list_elem* pelem = list_traversal(&thread_all_list, pid_check, pid);
	if(pelem == NULL) {
		return NULL;
	}
	struct task_struct* thread = elem2entry(struct task_struct, all_list_tag, pelem);
	return thread;
}

struct task_struct* tid2thread(int32_t tid) {
	struct list_elem* pelem = list_traversal(&thread_all_list, tid_check, tid);
	if(pelem == NULL) {
		return NULL;
	}
	struct task_struct* thread = elem2entry(struct task_struct, all_list_tag, pelem);
	return thread;
}

extern void init(void);

void thread_init(void) {
	put_str("thread_init start.\n");
	
	list_init(&thread_ready_list);
	list_init(&thread_all_list);
	pid_pool_init();
	process_execute(init, "init");
	make_main_thread();
	idle_thread = thread_start("idle", 10, idle, NULL);
	
	put_str("thread_init done.\n");
}

void thread_block(enum task_status stat) {
	ASSERT(( (stat == TASK_BLOCKED)\
				|| (stat == TASK_WAITING)\
				|| (stat == TASK_HANGING)));
	enum intr_status old_status = intr_disable();
	struct task_struct* cur_thread = running_thread();
	cur_thread->status = stat;
	schedule();
	intr_set_status(old_status);
}

void thread_unblock(struct task_struct* pthread) {
	enum intr_status old_status = intr_disable();
	ASSERT(( (pthread->status == TASK_BLOCKED)\
				|| (pthread->status == TASK_WAITING)\
				|| (pthread->status == TASK_HANGING)));
	if(pthread->status != TASK_READY) {
		ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
		if(elem_find(&thread_ready_list, &pthread->general_tag)) {
			PANIC("thread_unblock: blocked thread in ready_list.\n");
		}
		list_push(&thread_ready_list, &pthread->general_tag);
		pthread->status = TASK_READY;
	}
	intr_set_status(old_status);
}

pid_t fork_pid(void) {
	return allocate_pid(0);
}

static void pad_print(char* buf, int32_t buf_len, void* ptr, char format) {
	pad_sprint(buf, buf_len, ptr, format);
	sys_write(stdout_no, buf, buf_len - 1);
}

static bool elem2thread_info(struct list_elem* pelem, int arg UNUSED) {
	struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
	char out_pad[10] = {0};

	pad_print(out_pad, 10, &pthread->pid, 'd');

	if(pthread->parent_pid == -1) {
		pad_print(out_pad, 10, "NULL", 's');
	} else {
		pad_print(out_pad, 10, &pthread->parent_pid, 'd');
	}
	
	pad_print(out_pad, 10, &pthread->tid, 'd');

	switch(pthread->status) {
		case TASK_RUNNING:
			pad_print(out_pad, 10, "RUNNING", 's');
			break;
		case TASK_READY:
			pad_print(out_pad, 10, "READY", 's');
			break;
		case TASK_BLOCKED:
			pad_print(out_pad, 10, "BLOCKED", 's');
			break;
		case TASK_WAITING:
			pad_print(out_pad, 10, "WAITING", 's');
			break;
		case TASK_HANGING:
			pad_print(out_pad, 10, "HANGING", 's');
			break;
		case TASK_DIED:
			pad_print(out_pad, 10, "DIED", 's');
	}

	pad_print(out_pad, 10, &pthread->elapsed_ticks, 'x');

	memset(out_pad, 0, 10);
	ASSERT(strlen(pthread->name) < 11);
	memcpy(out_pad, pthread->name, strlen(pthread->name));
	strcat(out_pad, "\n");
	sys_write(stdout_no, out_pad,strlen(out_pad));
	return false;
}

void sys_ps(void) {
	char* ps_title = "PID      PPID     TID       STAT     TICKS    COMMAND\n";
	sys_write(stdout_no, ps_title, strlen(ps_title));
	list_traversal(&thread_all_list, elem2thread_info, 0);
}
