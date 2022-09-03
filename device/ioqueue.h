#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H
#include "stdint.h"
#include "thread.h"
#include "sync.h"

struct ioqueue {
	struct lock lock;
	struct task_struct* producer;
	struct task_struct* consumer;
	uint32_t bufsize;
	char* buf;
	int32_t head;
	int32_t tail;
};

int32_t ioqueue_init(struct ioqueue* ioq, uint32_t size);
bool ioq_full(struct ioqueue* ioq);
char ioq_getchar(struct ioqueue* ioq);
void ioq_putchar(struct ioqueue* ioq, char byte);
uint32_t ioq_length(struct ioqueue* ioq);
#endif
