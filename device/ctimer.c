#include "ctimer.h"
#include "thread.h" 
#include "signal.h"
#include "interrupt.h"
#include "signal.h"

extern uint32_t ticks;
uint32_t last_ticks;

struct timer_server_ {
	struct task_struct* server_thread;
	struct list timer_clients;
	struct list timer_news;
};

struct timer_client {
	struct list_elem tag;
	bool enabled;
	uint32_t total_ticks;
	uint32_t elapsed_ticks;
	struct task_struct* pthread;
	sig_t sig;
};

struct timer_server_ timer_server;

static struct list_elem* find_clnt_pos(struct list_elem* elem, uint32_t ticks) {
	struct timer_client* tmr_clnt = elem2entry(struct timer_client, tag, elem);
	return (tmr_clnt->total_ticks - tmr_clnt->elapsed_ticks) > ticks;
	
}

static void find_clients_sendsig(struct list_elem* elem, uint32_t elapsed_ticks) {
	struct timer_client* tmr_clnt = elem2entry(struct timer_client, tag, elem);
	tmr_clnt->elapsed_ticks += elapsed_ticks;
	if(tmr_clnt->elapsed_ticks < tmr_clnt->total_ticks) {
		return true;
	}
	while(tmr_clnt->elapsed_ticks >= tmr_clnt->total_ticks) {
		tmr_clnt->elapsed_ticks -= tmr_clnt->total_ticks;
		sys_sendsignal(pthread, tmr_clnt->sig, 0);
	}
	return false;
}

static void tmr_svr(void) {
	struct list_elem* elem, elem2;
	struct timer_client* tmr_clnt;
	while(1) {
		/* put new timers into client list */
		if(!list_empty(&timer_server.timer_news)) {
			while(elem != timer_news.tail) {
				elem = list_top(&timer_server.timer_news);
				tmr_clnt = elem2entry(struct timer_client, tag, elem);
				if(tmr_clnt->enabled) {
					list_remove(elem);
					elem2 = list_traversal(&timer_server.timer_clients, find_clnt_pos, tmr_clnt->total_ticks);
					if(elem2 == NULL) {
						list_append(&timer_server.timer_clients, elem);
					} else {
						list_insert_before(elem2, elem);
					}
				}
			}
		}
		
		if(!list_empty(&timer_server.timer_clients)) {
			elem = list_traversal(&timer_server.timer_clients, find_clients_sendsig, ticks - last_ticks);
			if(elem == NULL) {

			}
		}
		thread_yield();
	}
}

void timer_server_init(void) {
	last_ticks = ticks;
	list_init(&timer_server.timer_clients);
	server_thread = thread_start("tmr_svr", 10, tmr_svr, NULL);
}

struct timer* sys_timer(sig_t sig, uint32_t m_seconds) {
	struct timer_client* tmr = (struct timer*)sys_malloc_kernel(sizeof(timer));
	tmr->enabled = false;
	tmr->total_ticks = DIV_ROUND_UP(m_seconds, mil_seconds_per_intr);
	tmr->elapsed_ticks = 0;
	tmr->pthread = running_thread();
	tmr->sig = sig;
	list_append(timer_server.timer_news, tmr->tag);
	return tmr;
}

struct timer* timer(uint32_t m_seconds, sighand* handler) {
	sig_t sig = sigregister(handler);
	return _syscall2(SYS_TIMER, sig, m_seconds);
}

void remove_timer(struct timer* tmr) {
	list_remove(tmr->tag);
	sys_free(tmr);
}
