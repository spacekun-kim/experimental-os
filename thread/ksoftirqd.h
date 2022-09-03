#ifndef __THREAD_KSOFTIRQD_H
#define __THREAD_KSOFTIRQD_H

#include "stdint.h"

enum SOFTIRQ_TYPE {
	NET_RX = 1,
	NET_TX,
	OTHER_IRQ
};

void softirq_queue_put(uint8_t irq_type);
void ksoftirqd_init(void);
#endif
