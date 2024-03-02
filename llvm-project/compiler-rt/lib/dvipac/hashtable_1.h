#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>

typedef struct {
	//unsigned long tag; //64-bit
	//unsigned int tag; //32-bit
	//unsigned long nElements;
	//int typeSize;
	unsigned int type_id;
	unsigned int mod;
	unsigned long pac;
} __attribute__((packed)) metadata;
//} metadata;

#define SIZE ((0x1 << 28) * sizeof(unsigned long))
//#define SIZE ((0x1 << 7) * sizeof(unsigned long))
#define MAPPING_PATTERN ((SIZE - 1) ^ (sizeof(unsigned long) -1))
#define SHIFT_BITS 2
#define GET_OFFSET(addr) ((((uintptr_t)(addr)) << SHIFT_BITS) & MAPPING_PATTERN)

__attribute__((visibility("default")))
extern void init_memory();
__attribute__((visibility("default")))
//extern void dvipac_set(void **fptr, unsigned long tag, unsigned int nElements, int typeSize); //64-bit
//extern void dvipac_set(void **fptr, unsigned int tag, unsigned int nElements, int typeSize); //32-bit
extern void dvipac_set(unsigned int tbi_tag, unsigned int type_id, unsigned int mod, unsigned long pac);
__attribute__((visibility("default")))
extern metadata* dvipac_get(unsigned int tbi_tag);
//__attribute__((visibility("default")))
//extern void dvipac_remove(int tbi_tag);

metadata *shadow_memory;
