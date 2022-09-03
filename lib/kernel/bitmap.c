#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"

void bitmap_init(struct bitmap* btmp) {
	memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {
	uint32_t byte_idx = bit_idx >> 3;
	uint32_t bit_offset = bit_idx & 0x7;
	return (btmp->bits[byte_idx] & (BITMAP_MASK << bit_offset));
}

int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {
	uint32_t byte_idx = 0;
	uint32_t btmp_bytes_len = btmp->btmp_bytes_len;
	while((btmp->bits[byte_idx] == 0xff) && (byte_idx < btmp_bytes_len)) {
		++byte_idx;
	}
	//not enough space
	if((byte_idx + (cnt >> 3)) >= btmp_bytes_len)
		return -1;
	uint32_t bit_offset = 0;
	uint32_t count = 0;
	int bit_idx_start = byte_idx << 3;
	while(byte_idx < btmp_bytes_len) {
		if(btmp->bits[byte_idx] & (BITMAP_MASK << bit_offset)) {
			count = 0;
		} else {
			++count;
		}
		if(count == cnt) {		
			return bit_idx_start - cnt + 1;
		}
		if(bit_offset == 7) {
			bit_offset = 0;
			++byte_idx;
		} else {
			++bit_offset;
		}
		++bit_idx_start;
	}

	return -1;
}

void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value) {
	ASSERT((value ==0) || (value == 1));
	uint32_t byte_idx = bit_idx >> 3;
	uint32_t bit_offset = bit_idx & 0x7;
	if(value) {
		btmp->bits[byte_idx] |= (BITMAP_MASK << bit_offset);
	} else {
		btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_offset);
	}
}
