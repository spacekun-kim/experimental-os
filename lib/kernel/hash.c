#include "hash.h"
#include "memory.h"
#include "debug.h"
#include "global.h"
#include "string.h"

void hash_init(HT* H, uint32_t cap, valType errVal) {
	ASSERT(cap < 256 && cap != 31);
	H->nodes = (HASH**)sys_malloc(cap * sizeof(HASH*));
	H->capacity = cap;
	H->size = 0;
	H->hash = string_hash;
	H->errorVal = errVal;
}

uint32_t string_hash(char* str, uint32_t cap) {
	uint32_t h = 0;
	while(*str) {
		h = 31 * h + (uint32_t)*str;
		++str;
	}
	return h % cap;
}

static HASH* create_node(keyType key, valType val) {
	HASH* ret = (HASH*)sys_malloc(sizeof(HASH));
	ret->key = key;
	ret->val = val;
	ret->next = NULL;
	return ret;
}

void hash_insert(HT* H, keyType key, valType val) {
	uint32_t idx = H->hash(key, H->capacity);
	if(H->nodes[idx] == NULL) {
		H->nodes[idx] = create_node(key, val);
	} else {
		HASH* prev_node = H->nodes[idx];
		while(prev_node->next != NULL) {
			if(!strcmp(prev_node->key, key)) {
				prev_node->val = val;
				return;
			}
			prev_node = prev_node->next;
		}
		prev_node->next = create_node(key, val);
	}
}

void hash_delete(HT* H, keyType key) {
	uint32_t idx = H->hash(key, H->capacity);
	if(H->nodes[idx] == NULL)
		return;
	if(!strcmp(H->nodes[idx]->key, key)) {
		sys_free(H->nodes[idx]);
		H->nodes[idx] = NULL;
		return;
	}
	HASH* prev_node = H->nodes[idx];
	while(prev_node->next != NULL) {
		if(!strcmp(prev_node->next->key, key)) {
			sys_free(prev_node->next);
			prev_node->next = NULL;
		}
		prev_node = prev_node->next;
	}
	return;
}

valType hash_get(HT* H, keyType key) {
	uint32_t idx = H->hash(key, H->capacity);
	HASH* node = H->nodes[idx];
	while(node != NULL) {
		if(!strcmp(node->key, key)) {
			return node->val;
		}
		node = node->next;
	}
	return H->errorVal;
}
