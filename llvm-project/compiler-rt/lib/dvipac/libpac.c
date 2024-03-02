#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
//#include <sys/prctl.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include "libpac.h"
#define PAC
//#define PACINST
//#define EOR
//#define EOR1
#define EOR2
//#define PACEXISTS
#define bitGet(value) (value >> 48)
#define bitGet2(value) (value >> 44)
//#define PRINTDEBUG
//#define PRINTDEBUG2
//#define PRINTDEBUG3
//#define PRINTDEBUGREPLACE
#define TAG_SHIFT            56

// Function to add PAC to a pointer.
//inline void __attribute__((always_inline)) addPAC (void** p) {
void addPACpp (void** p, unsigned int tbi_tag) {
#ifdef PACEXISTS
	if(bitGet((unsigned long long) *p)){
		#ifdef PRINTDEBUG3
		printf("ADD SKIP &: %p\n", *p);
		#endif
        return;
    }
#endif
#ifndef PAC
	printf("addPAC &: %p\n", *p);
#endif
#ifdef PAC
#ifdef PRINTDEBUG
	printf("addPAC &: %p\n", *p);
#endif
	metadata* meta = dvipac_get(tbi_tag);
	//printf("Metadata pointer: %p\n", meta);
	//No hashtable
	//metadata meta;
    //meta.tag = 23;
    //meta.nElements = 1;
    //meta.typeSize = 8;
//	printf("addPAC Tag: %ld\n", meta->tag);
	if (!meta->mod)
//	if (!meta.tag)
	{
		//fprintf(stderr, "error: no tag exists\n");
		//exit(EXIT_FAILURE);
		//TEMP: Set meta to 0.
		unsigned int type_id = 0;
		dvipac_set(tbi_tag, type_id, 24, 0);
		//No hashtable
	    //meta.tag = 23;
	    //meta.nElements = 1;
   		//meta.typeSize = 8;
	}

//	printf("addPAC pointerb: %p\n", *p);
	unsigned long mod; //64-bit
//	unsigned int mod; //32-bit
//	mod = (long)p ^ meta->tag;
	mod = meta->mod;
	
/*
	if(*p){
		//mod = (*(long*)*p) ^ meta->tag;
		mod = meta->tag;
		//mod = meta.tag;
	}
	else{
		mod = meta->tag;
		//mod = meta.tag;
	}
*/
#ifdef PACINST
//	printf("PACINST\n");
	__asm volatile ("pacia %x[pointer], %x[modifier]\n"
			: [pointer] "+r" (*p)
			: [modifier] "r"(mod)
			);

	unsigned long pac = bitGet2((unsigned long long) *p);
	meta->pac = pac;
#endif
#ifdef EOR
	long* temp;
	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (2)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (3)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (4)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "r"(mod)
			);
	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (2)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (3)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (4)
			);

#endif
#ifdef EOR1
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
#endif
#ifdef EOR2
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
#endif
#ifdef PRINTDEBUG2
	printf("addPAC pointer: %p\n", *p);
	printf("addPAC tbi_tag: %d\n", tbi_tag);
	printf("addPAC pac: %p\n", pac);
	printf("addPAC type: %ld\n", meta->type_id);
	printf("addPAC modifer: %d\n", mod);
#endif
#endif
}

// Function to authenticate PAC to a pointer.
//inline void __attribute__((always_inline)) authenticatePAC (void** p) {
void authenticatePACpp (void** p, unsigned int tbi_tag) {
#ifdef PACEXISTS
	if(!bitGet((unsigned long long) *p)){
		#ifdef PRINTDEBUG3
		printf("AUTH SKIP &: %p\n", *p);
		#endif
        return;
    }
#endif
#ifndef PAC
	printf("authenticatePAC &: %p\n",*p);
#endif
#ifdef PAC
#ifdef PRINTDEBUG
	printf("authenticatePAC &: %p\n",*p);
#endif
	metadata* meta = dvipac_get(tbi_tag);
	//No hashtable
    //metadata meta;
    //meta.tag = 23;
    //meta.nElements = 1;
    //meta.typeSize = 8;
//	printf("authenticatePAC &: %p\n",p);
//	printf("authenticatePAC Tag: %ld\n", meta->tag);
	if (!meta->mod)
//	if (!meta.tag)
	{
		//fprintf(stderr, "error: no tag exists\n");
		//exit(EXIT_FAILURE);
		//TEMP: Set meta to 0.
		unsigned int type_id = 0;
        dvipac_set(tbi_tag, type_id, 24, 0);
		//meta.tag = 23;
        //meta.nElements = 1;
        //meta.typeSize = 8;
	}
	//void** temp = p;
	//printf("authenticatePAC Temp: %p\n", temp);
#ifdef PACINST
	//__asm volatile("xpaci %x[pointer]"
	//		: [pointer] "+r" (temp)
	//		);
#endif
	//printf("authenticatePAC TempX: %p\n", temp);
	unsigned long mod; //64-bit
//	unsigned int mod; //32-bit
//	mod = (long) p ^ meta->tag;
	mod = meta->mod;
/*
	if(*p){
		//mod = *temp ^ meta->tag;
		mod = meta->tag;
		//mod = meta.tag;
	}
	else{
		mod = meta->tag;
		//mod = meta.tag;
	}
*/
#ifdef PACINST
	unsigned long pac = meta->pac;
	unsigned long pac_mask = ~pac;
	pac_mask = pac_mask << 44;
	pac_mask = ~pac_mask;
	*p = (char *)((unsigned long)*p & pac_mask);
	__asm volatile ("autia %x[pointer], %x[modifier]\n"
			: [pointer] "+r" (*p)
			: [modifier] "r"(mod) 
			);
#endif
#ifdef EOR
	long* temp1;
	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp1)
			: [pointer2] "r" (*p), [modifier] "L" (2)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp1)
			: [pointer2] "r" (*p), [modifier] "L" (3)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp1)
			: [pointer2] "r" (*p), [modifier] "L" (4)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp1)
			: [pointer2] "r" (*p), [modifier] "r"(mod)
			);
	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp1)
			: [pointer2] "r" (*p), [modifier] "L" (2)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp1)
			: [pointer2] "r" (*p), [modifier] "L" (3)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp1)
			: [pointer2] "r" (*p), [modifier] "L" (4)
			);

#endif
#ifdef EOR1
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
    __asm volatile ("eor x15, x15, x15\n");
#endif
#ifdef EOR2
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
    __asm volatile ("eor x18, x18, x18\n");
#endif
#ifdef PRINTDEBUG2
	printf("authenticatePAC pointer: %p\n", *p);
	printf("authenticatePAC tbi_tag: %d\n", tbi_tag);
	printf("authenticatePAC pac %p\n", meta->pac);
	printf("authenticatePAC type: %ld\n", meta->type_id);
	printf("authenticatePAC modifer: %d\n", mod);
//	printf("authenticatePAC pointer: %p\n", *p);
#endif
#endif
}

// Function that inserts metadata into the hashtable.
//inline void __attribute__((always_inline)) setMetadata(void* p, unsigned int nElements, int typeSize){
void setMetadata(unsigned int type_id, unsigned int tbi_tag, unsigned int mod){
#ifndef PAC
	printf("setMetadata &: %d %d %d\n", type_id, tbi_tag, mod);
#endif
#ifdef PAC
#ifdef PRINTDEBUG
	printf("setMetadata &: %d %d %d\n", type_id, tbi_tag, mod);
#endif
//	if (metadataExists(tbi_tag, type_id))
//	{
//		return;
//	}	
	//unsigned long tag; //64-bit
//	unsigned int tag; //32-bit
	//tag = rand() & 0xffffffffffffffff;
	//tag = 23;
	dvipac_set(tbi_tag, type_id, mod, 0);
#ifdef PRINTDEBUG2
	printf("setMetadata Tag: %ld\n", tbi_tag);
	printf("setMetadata type id: %ld\n", type_id);
	printf("setMetadata modifier: %ld\n", mod);
#endif
#endif
}

// Function that detects if metadata exists.
//inline bool __attribute__((always_inline)) metadataExists(void* p){
bool metadataExists(unsigned int tbi_tag, unsigned int type_id){
#ifndef PAC
//	printf("metadataExists &: %p\n", p);
	if (tbi_tag == 0){
		return false;
	}
	else{
		return true;
	}
#endif
#ifdef PAC
	metadata* meta = dvipac_get(tbi_tag);
//	printf("metadataExists Tag: %ld\n", meta->tag);
	if (!meta->mod){
		return false;
	}
	else{
		return true;
	}

	if (meta->type_id != type_id){
		return false;
	}
	else{
		return true;
	}
#endif
}

/*// Function to convert decimal to hex
unsigned int convDecToHex(unsigned int n){//n>=0;
    unsigned int hex = 0;
    unsigned int base = 1;
    for(; n ; n /= 10, base *= 16){
        hex += (n % 10) * base;
    }
    return hex;
}
*/

// Function that inserts the Compact Equivalent (CE) to the top bits of the pointer
void addCEtbi(void** p, unsigned int tbi_tag){
	unsigned long tbiTag = tbi_tag;
	void* ptr = *p;
	ptr = (void *)((unsigned long)ptr | (tbiTag << TAG_SHIFT));
//	printf("Pointer with tag: %p\n", ptr);
	*p = ptr;
#ifdef PRINTDEBUG
	printf("TBI Tag: %lu\n", tbiTag);
	printf("Pointer with tag: %p\n", ptr);
	printf("Deref pointer with tag: %p\n", *p);
#endif
}

/*
// Function that inserts metadata into the hashtable.
//inline void __attribute__((always_inline)) setMetadataObj(void** p, unsigned int nElements, int typeSize){
void setMetadataObj(void** p, unsigned int nElements, int typeSize){
#ifndef PAC
	printf("setMetadataObj &: %p %d %d\n", *p, nElements, typeSize);
#endif
#ifdef PAC
#ifdef PRINTDEBUG
	printf("setMetadataObj &: %p\n", *p);
#endif
	if (metadataExists(*p))
	{
		return;
	}	
	unsigned long tag; //64-bit
	//unsigned int tag; //32-bit
	//tag = rand() & 0xffffffffffffffff;
	tag = 23;
	dvipac_set(*p, tag, nElements, typeSize);

//	printf("setMetadataObj Tag: %ld\n", tag);
//	printf("setMetadataObj nElements: %ld\n", nElements);
//	printf("setMetadata typeSize: %ld\n", typeSize);
#endif
}
*/


/*
// Function to add PAC to a pointer.
//inline void __attribute__((always_inline)) addPACNull (void** p) {
void addPACNull (void** p) {
#ifdef PACEXISTS
	if(bitGet((unsigned long long) *p)){
//		printf("ADDNULL SKIP: &: %p\n", *p);
        return;
    }
#endif
#ifndef PAC
	printf("addPACNull &: %p\n", *p);
#endif
#ifdef PAC
#ifdef PRINTDEBUG
	printf("addPACNull &: %p\n", *p);
#endif
	metadata* meta = dvipac_get(*p);
//	printf("addPACNull Tag: %ld\n", meta->tag);
	if (!meta->tag)
	{
		//fprintf(stderr, "error: no tag exists\n");
		//exit(EXIT_FAILURE);
		//TEMP: Set meta to 0.
		dvipac_set(*p, 23, 1, 8);
	}

//	printf("addPACNull pointerb: %p\n", *p);
	unsigned long mod = meta->tag;
#ifdef PACINST
	__asm volatile ("pacia %x[pointer], %x[modifier]\n"
			: [pointer] "+r" (*p)
			: [modifier] "r"(mod)
			);
#endif
#ifdef EOR
	long* temp;
	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (2)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (3)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (4)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "r"(mod)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (2)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (3)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (4)
			);

#endif
//	printf("addPACNull modifer: %ld\n", mod);
//	printf("addPACNull pointer: %p\n", *p);
#endif
}

// Function to authenticate PAC to a pointer.
//inline void __attribute__((always_inline)) authenticatePACNull (void** p) {
void authenticatePACNull (void** p) {
#ifdef PACEXISTS
	if(!bitGet((unsigned long long) *p)){
//		printf("AUTHNULL SKIP &: %p\n", *p);
        return;
    }
#endif
#ifndef PAC
	printf("authenticatePACNull &: %p\n",*p);
#endif
#ifdef PAC
#ifdef PRINTDEBUG
	printf("authenticatePACNull &: %p\n",*p);
#endif
	metadata* meta = dvipac_get(*p);
//	printf("authenticatePACNull Tag: %ld\n", meta->tag);
	if (!meta->tag)
	{
		//fprintf(stderr, "error: no tag exists\n");
		//exit(EXIT_FAILURE);
		//TEMP: Set meta to 0.
		dvipac_set(*p, 23, 1, 8);
	}

	unsigned long mod = meta->tag;
#ifdef PACINST
	__asm volatile ("autia %x[pointer], %x[modifier]\n"
			: [pointer] "+r" (*p)
			: [modifier] "r"(mod) 
			);
#endif
#ifdef EOR
	long* temp;
	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (2)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (3)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (4)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "r"(mod)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (2)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (3)
			);

	__asm volatile ("eor %x[pointer1], %x[pointer2], %x[modifier]\n"
			: [pointer1] "+r" (temp)
			: [pointer2] "r" (*p), [modifier] "L" (4)
			);

#endif
//	printf("authenticatePACNull modifer: %ld\n", mod);
//	printf("authenticatePACNull pointer: %p\n", *p);

#endif
}
*/


/*
// Function that removes metadata from the hashtable.
//inline void __attribute__((always_inline)) removeMetadata(void** p){
void removeMetadata(void** p){
#ifndef PAC
	printf("removeMetadata &: %p\n", *p);
#endif
#ifdef PAC
#ifdef PRINTDEBUG
	printf("removeMetadata &: %p\n", *p);
#endif
	metadata* meta = dvipac_get(*p);
	if (!meta->tag)
	{
//		fprintf(stderr, "error: no tag exists\n");
//		exit(EXIT_FAILURE);
//		TEMP: Set meta to 0.
		dvipac_set(*p, 23, 1, 8);
	}
//	printf("removeMetadata &: %p %d %d\n", *p, meta->nElements, meta->typeSize);
//	printf("removeTag Tag: %ld\n", meta->tag);
	unsigned nElements = meta->nElements;
	unsigned typeSize = meta->typeSize;
	for(int i = 0; i < nElements; i++){
		dvipac_remove(*p);
		(*p) = (*p) + typeSize;
//		printf("removeMetadata loop: %p\n", *p);
	}
#endif
}

// Function that removes metadata from the hashtable.
//inline void __attribute__((always_inline)) removeMetadataStack(void* p){
void removeMetadataStack(void* p){
#ifndef PAC
//	printf("removeMetadata &: %p\n", p);
#endif
#ifdef PAC
#ifdef PRINTDEBUG
	printf("removeMetadataStack &: %p\n", p);
#endif
	metadata* meta = dvipac_get(p);
	if (!meta->tag)
	{
//		fprintf(stderr, "error: no tag exists\n");
//		exit(EXIT_FAILURE);
//		TEMP: Set meta to 0.
		//dvipac_set(p, 23, 1, 8);
	}
//	printf("removeMetadata &: %p %d %d\n", p, meta->nElements, meta->typeSize);
//	printf("removeTag Tag: %ld\n", meta->tag);
	unsigned nElements = meta->nElements;
	unsigned typeSize = meta->typeSize;
	for(int i = 0; i < nElements; i++){
		dvipac_remove(p);
		(p) = (p) + typeSize;
//		printf("removeMetadata loop: %p\n", p);
	}
#endif
}
*/

/*
// Function that replaces pointer with already PACed pointer
//inline void __attribute__((always_inline)) replaceWithSignedPointer(void** p, void* signed_p){
void replaceWithSignedPointer(void** p, void* signed_p){
#ifdef PRINTDEBUGREPLACE
	printf("Replace: Pointer&: %p\n", p);
	printf("Replace: PACed Pointer: %p\n", signed_p);
	printf("Replace: Pointer: %p\n", *p);
//	printf("PACed Pointer: %p\n", *signed_p);
#endif

#ifdef PACEXISTS
	if(!bitGet((unsigned long long) signed_p)){
		#ifdef PRINTDEBUG3
		printf("REPLACE SKIP &: %p\n", signed_p);
		#endif
        return;
    }
#endif

	*p = signed_p;

#ifdef PRINTDEBUGREPLACE
	printf("Replace after: Pointer: %p\n", *p);
#endif
}

bool isPointerBad(void *p)
{
   int fh = open(p, 0, 0);
   int e = errno;

   if (-1 == fh && e == EFAULT)
   {
      //printf( "bad pointer: %p\n", p );
      return true;
   }
   else if (fh != -1)
   {
      close(fh);
   }

   //printf( "good pointer: %p\n", p );
   return false;
}
*/

// Function that retrieves a tag.
//static inline unsigned long __attribute__((always_inline)) getTag(void* p){
//	metadata* meta = dvipac_get(p);
//	printf("getTag: %ld\n", meta->tag);
//	return meta->tag;
//}
// Function that sets a specific tag.
//static inline void __attribute__((always_inline)) setTag(void* p, unsigned long tag){
//	printf("setTag: %ld\n", tag);
//	dvipac_set(p, tag);
//}

