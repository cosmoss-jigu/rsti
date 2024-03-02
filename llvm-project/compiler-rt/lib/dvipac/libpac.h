#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <stdint.h>
#include "hashtable_1.h"

__attribute__((visibility("default")))
extern void addPACpp (void** p, unsigned int tbi_tag);
__attribute__((visibility("default")))
extern void authenticatePACpp (void** p, unsigned int tbi_tag);
//__attribute__((visibility("default")))
//extern void addPACNull (void** p);
//__attribute__((visibility("default")))
//extern void authenticatePACNull (void** p);
__attribute__((visibility("default")))
extern void setMetadata(unsigned int type_id, unsigned int tbi_tag, unsigned int mod);
//__attribute__((visibility("default")))
//extern void setMetadataObj(void** p, unsigned int nElements, int typeSize);
//__attribute__((visibility("default")))
//extern void removeMetadata(void** p);
//__attribute__((visibility("default")))
//extern void removeMetadataStack(void* p);
__attribute__((visibility("default")))
extern bool metadataExists(unsigned int tbi_tag, unsigned int type_id);
//__attribute__((visibility("default")))
//extern void replaceWithSignedPointer(void** p, void* signed_p);
//__attribute__((visibility("default")))
//extern bool isPointerBad(void* p);
//static inline unsigned long __attribute__((always_inline)) getTag(void* p);
//static inline void __attribute__((always_inline)) setTag(void* p, unsigned long tag);
__attribute__((visibility("default"	)))
extern void addCEtbi(void** p, unsigned int tbi_tag);
