#include "hashtable_1.h"


//inline void __attribute__((always_inline)) init_memory()

//inline void __attribute__((constructor)) init_memory()
void __attribute__((constructor)) init_memory()
{
	
	unsigned long long size = SIZE;

	void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);

	if (p == (void*)-1){
		perror("Failed to mmap.");
		exit(1);
	}

	if(shadow_memory)
		return;

	shadow_memory = (metadata*) p;
//	metadata metaType;
//	printf("Size of meta: %zu bytes\n", sizeof(metaType));
//	printf("init_memory\n");
}


//inline void __attribute__((always_inline)) dvipac_set(void **fptr, unsigned int tag, unsigned int nElements, int typeSize) //32-bit

//inline void __attribute__((always_inline)) dvipac_set(void **fptr, unsigned long tag, unsigned int nElements, int typeSize) //64-bit
void dvipac_set(unsigned int tbi_tag, unsigned int type_id, unsigned int mod, unsigned long pac)
{
	unsigned long offset = GET_OFFSET(tbi_tag) + sizeof(unsigned long) & (SIZE -1);
	metadata *ptr = ((metadata *)(((unsigned char*) shadow_memory) + offset));
	ptr->type_id = type_id;
	ptr->mod = mod;
	ptr->pac = pac;
	//ptr->typeSize = typeSize;
	//printf("dvipac_set %p %ld %ld\n", ptr, ptr->type_id, ptr->mod);
}

//inline metadata* __attribute__((always_inline)) dvipac_get(void **fptr) 
metadata* dvipac_get(unsigned int tbi_tag)
{
	unsigned long offset = GET_OFFSET(tbi_tag) + sizeof(unsigned long) & (SIZE -1);
	metadata *ptr = ((metadata *)(((unsigned char*) shadow_memory) + offset));
	//printf("dvipac_get %p %ld %ld\n", ptr, ptr->type_id, ptr->mod);
	return ptr;
}


//inline void __attribute__((always_inline)) dvipac_remove(void **fptr)
/*void dvipac_remove(void **fptr)
{
	unsigned long offset = GET_OFFSET(fptr) + sizeof(unsigned long) & (SIZE -1);
	metadata *ptr = ((metadata *)(((unsigned char*) shadow_memory) + offset));
//	printf("dvipac_remove: %ld %ld %d\n", ptr->tag, ptr->nElements, ptr->typeSize);
	ptr->tag = 0;
	ptr->nElements = 0;
	ptr->typeSize = 0;
}
*/
