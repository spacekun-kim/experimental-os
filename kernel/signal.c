#include "signal.h"
#include "global.h"
#include "syscall.h"
#include "string.h"

uint32_t _arg;

static sig_t sigq_next(sig_t idx) {
	return (++idx) % SIG_QUEUE_MAX_CNT;
}

static bool sigq_empty(struct task_struct* thread) {
	return thread->sig_head == thread->sig_tail;
}

static bool sigq_full(struct task_struct* thread) {
	return sigq_next(thread->sig_head) == thread->sig_tail;
}

static sig_t sigq_get(struct task_struct* thread) {
	if(sigq_empty(thread)) {
		return -1;
	}
	sig_t ret = thread->sig_queue[thread->sig_tail];
	thread->sig_tail = sigq_next(thread->sig_tail);
	return ret;
}

static int32_t sigq_put(struct task_struct* thread, sig_t sig, uint32_t arg) {
	if(sigq_full(thread)) {
		return -1;
	}
	thread->sig_queue[thread->sig_head] = sig;
	thread->sig_arg[thread->sig_head] = arg;
	thread->sig_head = sigq_next(thread->sig_head);
	return 0;
}

/*
void sig_init(struct task_struct* thread) {
	thread->in_event = false;
	memset(thread->sig_queue, 0, SIG_QUEUE_MAX_CNT * sizeof(sig_t));
	thread->sig_head = 0;
	thread->sig_tail = 0;
}
*/

sighand* check_signal(void) {
	struct task_struct* cur_thread = running_thread();
	if(cur_thread->in_event || sigq_empty(cur_thread)) {
		return NULL;
	}
	sig_t sig = sigq_get(cur_thread);
	if(sig == -1) {
		return NULL;
	}
	cur_thread->in_event = true;
	_arg = cur_thread->sig_arg[sig];
	return cur_thread->handler[sig];
}

uint32_t get_arg(void) {
	return _arg;
}

void backup_intr_stack(bool cpl_down, void* esp) {
	struct task_struct* cur_thread = running_thread();
	cur_thread->backup_esp = esp;
	if(!cpl_down) {
		cur_thread->main_mission_stack.esp = 0;
		return;
	}
	void* dst = (void*)&cur_thread->main_mission_stack;
	void* src = esp;
	uint32_t len = sizeof(struct intr_stack);
	memcpy(dst, src, len);
}

void signal_handler(sighand* func, uint32_t arg) {
	func(arg);
	sigreturn();
}

sig_t sys_sigregister(sighand* func) {
	struct task_struct* cur_thread = running_thread();
	
	sig_t i;
	for(i = 0; i < SIG_CNT; ++i) {
		if(cur_thread->handler[i] == NULL) {
			cur_thread->handler[i] = func;
			return i;
		}
	}
	return -1;
}

void sys_sigretister_(sig_t sig, sighand* func) {
	struct task_struct* cur_thread = running_thread();
	cur_thread->handler[sig] = func;
}

void sys_sigrelease(sig_t sig) {
	if((sig < 0) || (sig >= SIG_CNT)) {
		return;
	}
	struct task_struct* cur_thread = running_thread();
	cur_thread->handler[sig] = NULL;
}

int32_t sys_sendsignal(pid_t tid, sig_t sig, uint32_t arg) {
	struct task_struct* thread = tid2thread(tid);
	return sys_sendsignal_(thread, sig, arg);
}

int32_t sys_sendsignal_(struct task_struct* target_thread, sig_t sig, uint32_t arg) {
	if(target_thread == NULL) {
		return -1;
	}
	if((sig < 0) || (sig >= SIG_CNT)) {
		return -1;
	}
	return sigq_put(target_thread, sig, arg);
}

void* event_over(void) {
	struct task_struct* cur_thread = running_thread();
	cur_thread->in_event = false;
	if(cur_thread->main_mission_stack.esp != 0) {
		void* src = (void*)&cur_thread->main_mission_stack;
		void* dst = (void*)cur_thread + PG_SIZE - sizeof(struct intr_stack);
		uint32_t len = sizeof(struct intr_stack);
		memcpy(dst, src, len);
	}
	return cur_thread->backup_esp;
}
