#include "ksoftirqd.h"
#include "global.h"
#include "interrupt.h"
#include "thread.h"
#include "ne2000.h"
#include "memory.h"
#include "socket.h"
#include "queue.h"
#include "ethernet.h"
#include "debug.h"
#include "thread.h"
#include "stdio-kernel.h"
#include "transport.h"
#include "network.h"

struct task_struct* ksoftirqd_thread;
struct Queue softirq_queue;

static void noirq_to_solve(void) {
	thread_block(TASK_WAITING);
}

void softirq_queue_put(uint8_t irq_type) {
	queue_put(&softirq_queue, irq_type);
	if(ksoftirqd_thread->status != TASK_WAITING) {
		return;
	}
	
	enum intr_status old_status = intr_disable();
	if(ksoftirqd_thread->status == TASK_WAITING) {
		thread_unblock(ksoftirqd_thread);
	}
	intr_set_status(old_status);
}

static void ksoftirqd(void* arg UNUSED) {
	uint8_t cur_irq;
	bool last_irq_solved = true;
	struct Message* msg;
	struct Message** msg_ptr = &msg;
	struct socket_* sock;

	while(1) {
		/* if last irq solved, get a new irq */
		if(last_irq_solved) {
			/* if no softirq needs to solve, schedule */
			while(queue_empty(&softirq_queue)) {
				noirq_to_solve();
			}
			enum intr_status old_status = intr_disable();
			cur_irq = queue_get(&softirq_queue);
			intr_set_status(old_status);
		}
		// printk("get an irq: %d\n", cur_irq);
		switch(cur_irq) {
			case NET_RX:
				/* get a buffer to receive */
				*msg_ptr = get_message_from_list(&msg_free_list, 0);
				if(*msg_ptr == NULL) {
					last_irq_solved = false;
					noirq_to_solve();
					continue;
				}
				ethernet_recv(msg_ptr);
				
				switch((*msg_ptr)->status) {
					case STATUS_DELIVER:
						//deliver packet to thread
						sock = listen_search(*msg_ptr);
						if(sock != NULL) {
							list_append(&sock->msg_recv_list, &(*msg_ptr)->tag);
						}
						break;
					case STATUS_FREEMSG:
						message_free(*msg_ptr);
						break;
					case STATUS_NOTASK:
						break;
					default:
						ASSERT("Should not be here!");
				}
				break;
			case NET_TX:
				*msg_ptr = get_message_from_list(&msg_send_list, 0);
				ASSERT(*msg_ptr != NULL);
				switch((*msg_ptr)->protocol) {
					case PRO_LOCAL:
						break;
					case PRO_ICMP:
						icmp_send(msg_ptr);
						break;
					case PRO_TCP:
						tcp_send(msg_ptr);
						break;
					case PRO_UDP:
						udp_send(msg_ptr);
				}
				// ethernet_send(msg_ptr);
				switch((*msg_ptr)->status) {
					case STATUS_FREEMSG:
						message_free(*msg_ptr);
						break;
					case STATUS_WAIT:
						break;
					case STATUS_NOTASK:
						break;
					default:
						ASSERT("Should not be here!");
				}
				break;
			default:
				printk("unknown irq\n");
		}
		last_irq_solved = true;
	}
}

void ksoftirqd_init(void) {
	/* init softirq queue */
	queue_init(&softirq_queue, 256);
	msg_init();
	arp_init();
	ksoftirqd_thread = thread_start("ksoftirqd", 10, ksoftirqd, NULL);
}
