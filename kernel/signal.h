#ifndef __KERNEL_SIGNAL_H
#define __KERNEL_SIGNAL_H

#define SIG_CNT 32
#define SIG_QUEUE_MAX_CNT 32
#include "stdint.h"
typedef int8_t sig_t;
typedef void sighand(uint32_t);
#include "thread.h"

// void sig_init(struct task_struct* thread);
sighand* check_signal(void);
uint32_t get_arg(void);
void backup_intr_stack(bool cpl_down, void* esp);
void signal_handler(sighand* func, uint32_t arg);
sig_t sys_sigregister(sighand* func);
void sys_sigretister_(sig_t sig, sighand* func);
void sys_sigrelease(sig_t sig);
extern void sys_sigreturn(void);
int32_t sys_sendsignal(pid_t tid, sig_t sig, uint32_t arg);
int32_t sys_sendsignal_(struct task_struct* target_thread, sig_t sig, uint32_t arg);
void* event_over(void);

#endif
