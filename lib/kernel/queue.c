#include "queue.h"
#include "memory.h"
#include "global.h"
#include "stdint.h"
#include "debug.h"
#include "interrupt.h"

int32_t queue_init(struct Queue* queue, uint32_t size) {
	queue->head = 0;
	queue->tail = 0;
	queue->size = size;
	lock_init(&queue->queue_lock);
	queue->data = sys_malloc(size);
	if(queue->data == NULL) {
		return -1;
	}
	return 0;
}

static uint32_t queue_next(struct Queue* queue, uint32_t idx) {
	return (++idx) % queue->size;
}

bool queue_empty(struct Queue* queue) {
	return queue->head == queue->tail;
}

static bool queue_full(struct Queue* queue) {
	return queue_next(queue, queue->head) == queue->tail;
}

int8_t queue_get(struct Queue* queue) {
	lock_acquire(&queue->queue_lock);
	if(queue_empty(queue)) {
		lock_release(&queue->queue_lock);
		return -1;
	}
	int8_t ret = queue->data[queue->tail];
	queue->tail = queue_next(queue, queue->tail);
	lock_release(&queue->queue_lock);
	return ret;
}

void queue_put(struct Queue* queue, int8_t val) {
	lock_acquire(&queue->queue_lock);
	if(queue_full(queue)) {
		lock_release(&queue->queue_lock);
		return;
	}
	queue->data[queue->head] = val;
	queue->head = queue_next(queue, queue->head);
	lock_release(&queue->queue_lock);
}
