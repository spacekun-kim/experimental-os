#include "ioqueue.h"
#include "interrupt.h"
#include "global.h"
#include "debug.h"
#include "memory.h"

int32_t ioqueue_init(struct ioqueue* ioq, uint32_t size) {
	lock_init(&ioq->lock);
	ioq->producer = ioq->consumer = (struct task_struct*)NULL;
	ioq->bufsize = size;
	ioq->buf = sys_malloc_kernel(size);
	if(ioq->buf == NULL) {
		return -1;
	}
	ioq->head = ioq->tail = 0;
	return 0;
}

static int32_t next_pos(struct ioqueue* ioq, int32_t pos) {
	return (pos + 1) % ioq->bufsize;
}

bool ioq_full(struct ioqueue* ioq) {
	ASSERT(intr_get_status() == INTR_OFF);
	return next_pos(ioq, ioq->head) == ioq->tail;
}

static bool ioq_empty(struct ioqueue* ioq) {
	ASSERT(intr_get_status() == INTR_OFF);
	return ioq->head == ioq->tail;
}

static void ioq_wait(struct task_struct** waiter) {
	ASSERT((*waiter == (struct task_struct*)NULL) && (waiter != (struct task_struct**)NULL));
	*waiter = running_thread();
	thread_block(TASK_BLOCKED);
}

static void wakeup(struct task_struct** waiter) {
	ASSERT(*waiter != (struct task_struct*)NULL);
	thread_unblock(*waiter);
	*waiter = (struct task_struct*)NULL;
}

char ioq_getchar(struct ioqueue* ioq) {
	ASSERT(intr_get_status() == INTR_OFF);
	
	while(ioq_empty(ioq)) {
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->consumer);
		lock_release(&ioq->lock);
	}

	char byte = ioq->buf[ioq->tail];
	ioq->tail = next_pos(ioq, ioq->tail);

	if(ioq->producer != (struct task_struct*)NULL) {
		wakeup(&ioq->producer);
	}
	return byte;
}

void ioq_putchar(struct ioqueue* ioq, char byte) {
	ASSERT(intr_get_status() == INTR_OFF);

	while(ioq_full(ioq)) {
		lock_acquire(&ioq->lock);
		ioq_wait(&ioq->producer);
		lock_release(&ioq->lock);
	}

	ioq->buf[ioq->head] = byte;
	ioq->head = next_pos(ioq, ioq->head);

	if(ioq->consumer != (struct task_struct*)NULL) {
		wakeup(&ioq->consumer);
	}
}

uint32_t ioq_length(struct ioqueue* ioq) {
	uint32_t len = 0;
	if(ioq->head >= ioq->tail) {
		len = ioq->head - ioq->tail;
	} else {
		len = ioq->bufsize - (ioq->tail - ioq->head);
	}
	return len;
}
