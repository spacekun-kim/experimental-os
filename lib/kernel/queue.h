#ifndef __LIB_RING_BUFFER_H
#define __LIB_RING_BUFFER_H

#include "stdint.h"
#include "sync.h"

struct Queue {
	struct lock queue_lock;
	uint32_t head;
	uint32_t tail;
	uint32_t size;
	int8_t* data;
};

int32_t queue_init(struct Queue* queue, uint32_t size);
bool queue_empty(struct Queue* queue);
int8_t queue_get(struct Queue* queue);
void queue_put(struct Queue* queue, int8_t val);
#endif
