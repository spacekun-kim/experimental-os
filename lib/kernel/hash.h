#ifndef __LIB_KERNEL_HASH_H
#define __LIB_KERNEL_HASH_H
#include "stdint.h"

typedef char* keyType;
typedef int32_t valType;

typedef struct hash_node {
	keyType key;
	valType val;
	struct hash_node* next;
}HASH;

typedef struct hash_table {
	HASH** nodes;
	uint32_t capacity;
	uint32_t size;
	uint32_t (*hash)(keyType key, uint32_t hashSize);
	valType errorVal;
}HT;

void hash_init(HT* H, uint32_t cap, valType errVal);
uint32_t string_hash(char* str, uint32_t cap);
void hash_insert(HT* H, keyType key, valType val);
void hash_delete(HT* H, keyType key);
valType hash_get(HT* H, keyType key);
#endif
